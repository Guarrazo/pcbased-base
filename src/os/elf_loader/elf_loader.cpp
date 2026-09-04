#include "os/elf_loader/elf_loader.h"
#include "os/elf_loader/elf32_format.h"
#include "core/log.h"
#include <cstring>
#include <algorithm>
#include <limits>

namespace pas::os {

using namespace elf32;

namespace {

// Lee un struct T desde 'image' en 'offset', comprobando limites. Devuelve
// false (y loguea) si no cabe -- nunca lee fuera del buffer del fichero.
template <typename T>
bool ReadAt(const uint8_t* image, size_t image_size, uint32_t offset, T& out) {
    if (static_cast<uint64_t>(offset) + sizeof(T) > image_size) {
        PAS_LOG_ERROR("ElfLoader", "Lectura fuera de rango en offset 0x%x (struct de %zu bytes, "
                                   "fichero de %zu bytes)", offset, sizeof(T), image_size);
        return false;
    }
    std::memcpy(&out, image + offset, sizeof(T));
    return true;
}

const char* StringAt(const uint8_t* image, size_t image_size, uint32_t strtab_offset,
                      uint32_t name_index) {
    uint64_t abs_offset = static_cast<uint64_t>(strtab_offset) + name_index;
    if (abs_offset >= image_size) return "";
    // No hay garantia de terminador dentro de limites -- se usa como
    // cadena C confiando en que el ELF es valido; en un ELF corrupto esto
    // podria leer de mas, pero el caso de uso es fichero local, no una
    // entrada de red no confiable.
    return reinterpret_cast<const char*>(image + abs_offset);
}

} // namespace

uint8_t* LoadedElf::AddressToPointer(uint32_t virtual_address) {
    if (virtual_address < base_address) return nullptr;
    uint64_t rel = static_cast<uint64_t>(virtual_address) - base_address;
    if (rel >= guest_memory.size()) return nullptr;
    return guest_memory.data() + rel;
}

std::optional<LoadedElf> ElfLoader::Load(const uint8_t* image, size_t image_size,
                                          SymbolResolver resolver, void* resolver_data) {
    Ehdr ehdr;
    if (!ReadAt(image, image_size, 0, ehdr)) return std::nullopt;

    if (std::memcmp(ehdr.e_ident, kMagic, 4) != 0) {
        PAS_LOG_ERROR("ElfLoader", "Magic ELF invalido");
        return std::nullopt;
    }
    if (ehdr.e_ident[4] != kClass32) {
        PAS_LOG_ERROR("ElfLoader", "Solo se soporta ELF32 en el MVP (docs/CPU_TRANSLATION.md) "
                                   "-- clase recibida: %u", ehdr.e_ident[4]);
        return std::nullopt;
    }
    if (ehdr.e_ident[5] != kDataLsb) {
        PAS_LOG_ERROR("ElfLoader", "Solo se soporta little-endian -- data recibida: %u",
                      ehdr.e_ident[5]);
        return std::nullopt;
    }
    if (ehdr.e_machine != kMachine386) {
        PAS_LOG_ERROR("ElfLoader", "Solo se soporta EM_386 -- machine recibida: %u",
                      ehdr.e_machine);
        return std::nullopt;
    }
    if (ehdr.e_type != kTypeExec && ehdr.e_type != kTypeDyn) {
        PAS_LOG_ERROR("ElfLoader", "Tipo de ELF no soportado (ni ET_EXEC ni ET_DYN): %u",
                      ehdr.e_type);
        return std::nullopt;
    }

    // --- Paso 1: recorrer PT_LOAD para calcular el rango de direcciones ---
    uint32_t min_vaddr = std::numeric_limits<uint32_t>::max();
    uint32_t max_vaddr_end = 0;
    std::vector<Phdr> load_segments;

    for (uint16_t i = 0; i < ehdr.e_phnum; ++i) {
        Phdr phdr;
        if (!ReadAt(image, image_size, ehdr.e_phoff + i * ehdr.e_phentsize, phdr)) {
            return std::nullopt;
        }
        if (phdr.p_type != kPtLoad) continue;

        load_segments.push_back(phdr);
        min_vaddr = std::min(min_vaddr, phdr.p_vaddr);
        uint64_t end = static_cast<uint64_t>(phdr.p_vaddr) + phdr.p_memsz;
        max_vaddr_end = static_cast<uint32_t>(std::max<uint64_t>(max_vaddr_end, end));
    }

    if (load_segments.empty()) {
        PAS_LOG_ERROR("ElfLoader", "El ELF no tiene ningun segmento PT_LOAD");
        return std::nullopt;
    }

    LoadedElf out;
    out.base_address = min_vaddr;
    out.entry_point = ehdr.e_entry;
    out.guest_memory.assign(max_vaddr_end - min_vaddr, 0); // zero-init cubre .bss automaticamente

    PAS_LOG_INFO("ElfLoader", "Rango de direcciones guest: 0x%08x - 0x%08x (%zu bytes), entry=0x%08x",
                min_vaddr, max_vaddr_end, out.guest_memory.size(), out.entry_point);

    // --- Paso 2: copiar el contenido de cada PT_LOAD ---
    for (const auto& phdr : load_segments) {
        if (phdr.p_filesz == 0) continue; // segmento solo-memoria (p.ej. bss puro)
        if (static_cast<uint64_t>(phdr.p_offset) + phdr.p_filesz > image_size) {
            PAS_LOG_ERROR("ElfLoader", "Segmento PT_LOAD fuera de rango del fichero "
                                       "(offset=0x%x, filesz=0x%x)", phdr.p_offset, phdr.p_filesz);
            return std::nullopt;
        }
        uint32_t dest_offset = phdr.p_vaddr - min_vaddr;
        std::memcpy(out.guest_memory.data() + dest_offset, image + phdr.p_offset, phdr.p_filesz);
    }

    // --- Paso 3: localizar .dynsym/.dynstr/.rel.* via section headers ---
    // (mas simple y directo que recorrer el segmento PT_DYNAMIC entrada a
    // entrada -- los binarios objetivo no son ficheros "solo con program
    // headers" tipicos de un core dump, tienen section headers completos).
    uint32_t dynsym_off = 0, dynsym_size = 0, dynsym_entsize = sizeof(Sym);
    uint32_t dynstr_off = 0;
    std::vector<Shdr> rel_sections;

    for (uint16_t i = 0; i < ehdr.e_shnum; ++i) {
        Shdr shdr;
        if (!ReadAt(image, image_size, ehdr.e_shoff + i * ehdr.e_shentsize, shdr)) {
            return std::nullopt;
        }
        if (shdr.sh_type == kShtDynsym) {
            dynsym_off = shdr.sh_offset;
            dynsym_size = shdr.sh_size;
            dynsym_entsize = shdr.sh_entsize ? shdr.sh_entsize : sizeof(Sym);
            // sh_link de .dynsym apunta a la seccion .dynstr correspondiente
            Shdr strtab_shdr;
            if (ReadAt(image, image_size, ehdr.e_shoff + shdr.sh_link * ehdr.e_shentsize, strtab_shdr)) {
                dynstr_off = strtab_shdr.sh_offset;
            }
        } else if (shdr.sh_type == kShtRel) {
            rel_sections.push_back(shdr);
        }
    }

    if (dynsym_off == 0 || dynstr_off == 0) {
        PAS_LOG_WARN("ElfLoader", "No se encontro .dynsym/.dynstr -- binario sin simbolos "
                                  "dinamicos (o estatico); no hay reubicaciones que resolver");
        return out;
    }

    uint32_t symbol_count = dynsym_entsize ? dynsym_size / dynsym_entsize : 0;
    PAS_LOG_INFO("ElfLoader", "%u simbolos dinamicos, %zu secciones de reubicacion",
                symbol_count, rel_sections.size());

    // --- Paso 4: aplicar reubicaciones ---
    for (const auto& rel_shdr : rel_sections) {
        uint32_t rel_count = rel_shdr.sh_entsize ? rel_shdr.sh_size / rel_shdr.sh_entsize : 0;
        for (uint32_t r = 0; r < rel_count; ++r) {
            Rel rel;
            if (!ReadAt(image, image_size, rel_shdr.sh_offset + r * rel_shdr.sh_entsize, rel)) {
                return std::nullopt;
            }

            uint32_t type = RelType(rel.r_info);
            uint32_t sym_index = RelSymIndex(rel.r_info);

            uint8_t* target = out.AddressToPointer(rel.r_offset);
            if (!target || target + 4 > out.guest_memory.data() + out.guest_memory.size()) {
                PAS_LOG_ERROR("ElfLoader", "Reubicacion apunta fuera del espacio guest (0x%08x)",
                              rel.r_offset);
                continue;
            }

            uint32_t addend = 0;
            std::memcpy(&addend, target, 4); // ELF32 REL: el addend esta en el propio destino

            if (type == kR386Relative) {
                uint32_t value = out.base_address + addend;
                std::memcpy(target, &value, 4);
                continue;
            }

            // El resto de tipos necesitan resolver un simbolo por nombre.
            Sym sym;
            if (sym_index >= symbol_count ||
                !ReadAt(image, image_size, dynsym_off + sym_index * dynsym_entsize, sym)) {
                PAS_LOG_ERROR("ElfLoader", "Indice de simbolo invalido en reubicacion: %u",
                              sym_index);
                continue;
            }
            const char* name = StringAt(image, image_size, dynstr_off, sym.st_name);

            std::optional<uint32_t> resolved;
            if (resolver) {
                resolved = resolver(name, resolver_data);
            }

            if (!resolved) {
                out.unresolved_symbols.push_back({name, rel.r_offset});
                PAS_LOG_WARN("ElfLoader", "Simbolo no resuelto: '%s' (usado en reubicacion 0x%08x, "
                                          "tipo=%u)", name, rel.r_offset, type);
                continue;
            }

            uint32_t value = 0;
            switch (type) {
                case kR386_32:     value = *resolved + addend; break;
                case kR386PC32:    value = *resolved + addend - rel.r_offset; break;
                case kR386GlobDat: value = *resolved; break;
                case kR386JmpSlot: value = *resolved; break;
                default:
                    PAS_LOG_WARN("ElfLoader", "Tipo de reubicacion no soportado todavia: %u "
                                              "(simbolo '%s') -- ver docs/CPU_TRANSLATION.md, "
                                              "alcance incremental", type, name);
                    out.unresolved_symbols.push_back({name, rel.r_offset});
                    continue;
            }
            std::memcpy(target, &value, 4);
        }
    }

    return out;
}

} // namespace pas::os
