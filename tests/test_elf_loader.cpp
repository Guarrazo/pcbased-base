// Test de host para os::ElfLoader -- construye un ELF32 sintetico minimo a
// mano (1 segmento PT_LOAD, 1 simbolo dinamico, 1 reubicacion de cada tipo
// relevante) para validar el parser sin depender de un binario real de
// Lindbergh (que no tenemos disponible en este entorno). Ver docs/ROADMAP.md,
// Nivel 1.
#include "os/elf_loader/elf_loader.h"
#include "os/elf_loader/elf32_format.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <optional>

using namespace pas::os;
using namespace pas::os::elf32;

namespace {

// Construye un ELF32 valido con:
//  - un PT_LOAD en vaddr 0x8000, tamano 0x100 (memsz > filesz para probar
//    que el .bss implicito queda a cero)
//  - .dynsym con 2 entradas (NULL + "resolved_symbol" + "missing_symbol")
//  - .rel con: R_386_RELATIVE, R_386_JMP_SLOT (resuelto), R_386_32 (no resuelto)
std::vector<uint8_t> BuildSyntheticElf(uint32_t& out_reloc_relative_addr,
                                        uint32_t& out_reloc_jmpslot_addr,
                                        uint32_t& out_reloc_unresolved_addr) {
    constexpr uint32_t kVaddr = 0x8000;
    constexpr uint32_t kFilesz = 0x40; // datos reales en el fichero
    constexpr uint32_t kMemsz  = 0x100; // >filesz -> el resto debe quedar a 0 (bss)

    // Reservamos dentro del segmento tres words de 4 bytes para las
    // reubicaciones, en offsets fijos dentro de kFilesz.
    out_reloc_relative_addr   = kVaddr + 0x10;
    out_reloc_jmpslot_addr    = kVaddr + 0x14;
    out_reloc_unresolved_addr = kVaddr + 0x18;

    std::vector<uint8_t> img;
    img.resize(4096, 0xCC); // relleno reconocible para detectar overruns

    auto put = [&](size_t offset, const void* data, size_t size) {
        assert(offset + size <= img.size());
        std::memcpy(img.data() + offset, data, size);
    };

    // --- Layout de offsets dentro del fichero ---
    constexpr size_t kEhdrOff = 0;
    constexpr size_t kPhdrOff = kEhdrOff + sizeof(Ehdr);
    constexpr size_t kSegDataOff = kPhdrOff + sizeof(Phdr);       // datos del PT_LOAD
    constexpr size_t kDynstrOff = kSegDataOff + kFilesz;
    constexpr size_t kDynsymOff = kDynstrOff + 64;                 // margen para las cadenas
    constexpr size_t kRelOff   = kDynsymOff + sizeof(Sym) * 3;
    constexpr size_t kShdrOff  = kRelOff + sizeof(Rel) * 3;

    // --- Ehdr ---
    Ehdr ehdr{};
    std::memcpy(ehdr.e_ident, kMagic, 4);
    ehdr.e_ident[4] = kClass32;
    ehdr.e_ident[5] = kDataLsb;
    ehdr.e_type = kTypeExec;
    ehdr.e_machine = kMachine386;
    ehdr.e_version = 1;
    ehdr.e_entry = kVaddr; // entry point = inicio del segmento, por simplicidad
    ehdr.e_phoff = kPhdrOff;
    ehdr.e_shoff = kShdrOff;
    ehdr.e_ehsize = sizeof(Ehdr);
    ehdr.e_phentsize = sizeof(Phdr);
    ehdr.e_phnum = 1;
    ehdr.e_shentsize = sizeof(Shdr);
    ehdr.e_shnum = 4; // [0]=NULL, [1]=.dynsym, [2]=.dynstr, [3]=.rel
    ehdr.e_shstrndx = 0;
    put(kEhdrOff, &ehdr, sizeof(ehdr));

    // --- Phdr (PT_LOAD) ---
    Phdr phdr{};
    phdr.p_type = kPtLoad;
    phdr.p_offset = kSegDataOff;
    phdr.p_vaddr = kVaddr;
    phdr.p_paddr = kVaddr;
    phdr.p_filesz = kFilesz;
    phdr.p_memsz = kMemsz;
    put(kPhdrOff, &phdr, sizeof(phdr));

    // Datos del segmento: los dejamos a 0 salvo un marcador reconocible al
    // principio, para poder comprobar que se copiaron bien.
    uint32_t marker = 0x1234ABCD;
    put(kSegDataOff, &marker, 4);

    // Las tres words donde caen las reubicaciones deben tener un addend
    // conocido en el fichero (formato REL: el addend vive en el propio
    // destino) -- el resto del segmento se dejo con el relleno 0xCC a
    // proposito para detectar overruns, pero AQUI necesitamos 0 explicito,
    // si no el addend seria 0xCCCCCCCC y las aserciones no significarian
    // lo que dicen significar.
    uint32_t zero = 0;
    put(kSegDataOff + 0x10, &zero, 4);
    put(kSegDataOff + 0x14, &zero, 4);
    put(kSegDataOff + 0x18, &zero, 4);

    // --- .dynstr: "\0resolved_symbol\0missing_symbol\0" ---
    const char* str_resolved = "resolved_symbol";
    const char* str_missing  = "missing_symbol";
    size_t off_resolved = 1; // offset 0 reservado (convencion ELF: nombre vacio)
    size_t off_missing  = off_resolved + std::strlen(str_resolved) + 1;
    put(kDynstrOff + off_resolved, str_resolved, std::strlen(str_resolved) + 1);
    put(kDynstrOff + off_missing, str_missing, std::strlen(str_missing) + 1);

    // --- .dynsym: [0]=NULL, [1]=resolved_symbol, [2]=missing_symbol ---
    Sym sym_null{};
    put(kDynsymOff + 0 * sizeof(Sym), &sym_null, sizeof(Sym));

    Sym sym_resolved{};
    sym_resolved.st_name = static_cast<uint32_t>(off_resolved);
    put(kDynsymOff + 1 * sizeof(Sym), &sym_resolved, sizeof(Sym));

    Sym sym_missing{};
    sym_missing.st_name = static_cast<uint32_t>(off_missing);
    put(kDynsymOff + 2 * sizeof(Sym), &sym_missing, sizeof(Sym));

    // --- .rel: RELATIVE, JMP_SLOT(sym=1), R_386_32(sym=2, sin resolver) ---
    Rel rel_relative{};
    rel_relative.r_offset = out_reloc_relative_addr;
    rel_relative.r_info = (0u << 8) | kR386Relative; // sym index irrelevante para RELATIVE
    put(kRelOff + 0 * sizeof(Rel), &rel_relative, sizeof(Rel));

    Rel rel_jmpslot{};
    rel_jmpslot.r_offset = out_reloc_jmpslot_addr;
    rel_jmpslot.r_info = (1u << 8) | kR386JmpSlot;
    put(kRelOff + 1 * sizeof(Rel), &rel_jmpslot, sizeof(Rel));

    Rel rel_unresolved{};
    rel_unresolved.r_offset = out_reloc_unresolved_addr;
    rel_unresolved.r_info = (2u << 8) | kR386_32;
    put(kRelOff + 2 * sizeof(Rel), &rel_unresolved, sizeof(Rel));

    // --- Shdrs ---
    Shdr shdr_null{};
    put(kShdrOff + 0 * sizeof(Shdr), &shdr_null, sizeof(Shdr));

    Shdr shdr_dynsym{};
    shdr_dynsym.sh_type = kShtDynsym;
    shdr_dynsym.sh_offset = kDynsymOff;
    shdr_dynsym.sh_size = sizeof(Sym) * 3;
    shdr_dynsym.sh_entsize = sizeof(Sym);
    shdr_dynsym.sh_link = 2; // indice de la seccion .dynstr
    put(kShdrOff + 1 * sizeof(Shdr), &shdr_dynsym, sizeof(Shdr));

    Shdr shdr_dynstr{};
    shdr_dynstr.sh_type = 3; // SHT_STRTAB (no comprobado por nuestro loader, solo el offset)
    shdr_dynstr.sh_offset = kDynstrOff;
    shdr_dynstr.sh_size = 64;
    put(kShdrOff + 2 * sizeof(Shdr), &shdr_dynstr, sizeof(Shdr));

    Shdr shdr_rel{};
    shdr_rel.sh_type = kShtRel;
    shdr_rel.sh_offset = kRelOff;
    shdr_rel.sh_size = sizeof(Rel) * 3;
    shdr_rel.sh_entsize = sizeof(Rel);
    put(kShdrOff + 3 * sizeof(Shdr), &shdr_rel, sizeof(Shdr));

    img.resize(kShdrOff + 4 * sizeof(Shdr));
    return img;
}

std::optional<uint32_t> FakeResolver(const std::string& name, void* /*user_data*/) {
    if (name == "resolved_symbol") return 0xDEAD0000u;
    return std::nullopt; // "missing_symbol" y cualquier otro quedan sin resolver
}

} // namespace

static void TestValidElfParsesAndLoadsSegment() {
    uint32_t rel_relative_addr, rel_jmpslot_addr, rel_unresolved_addr;
    auto img = BuildSyntheticElf(rel_relative_addr, rel_jmpslot_addr, rel_unresolved_addr);

    ElfLoader loader;
    auto result = loader.Load(img.data(), img.size(), FakeResolver, nullptr);
    assert(result.has_value());

    assert(result->base_address == 0x8000);
    assert(result->entry_point == 0x8000);
    assert(result->guest_memory.size() == 0x100); // memsz, no filesz

    uint32_t marker = 0;
    std::memcpy(&marker, result->guest_memory.data(), 4);
    assert(marker == 0x1234ABCD);

    // Zona mas alla de filesz (bss implicito) debe estar a cero, no al
    // relleno 0xCC del fichero.
    assert(result->guest_memory[0x100 - 1] == 0);

    std::printf("OK: TestValidElfParsesAndLoadsSegment\n");
}

static void TestRelativeRelocationAppliesBaseAddress() {
    uint32_t rel_relative_addr, rel_jmpslot_addr, rel_unresolved_addr;
    auto img = BuildSyntheticElf(rel_relative_addr, rel_jmpslot_addr, rel_unresolved_addr);

    ElfLoader loader;
    auto result = loader.Load(img.data(), img.size(), FakeResolver, nullptr);
    assert(result.has_value());

    uint8_t* p = result->AddressToPointer(rel_relative_addr);
    assert(p != nullptr);
    uint32_t value;
    std::memcpy(&value, p, 4);
    // R_386_RELATIVE = base_address + addend; el addend original en ese
    // word era 0 (memoria zero-init, offset > filesz de 0x40).
    assert(value == result->base_address);

    std::printf("OK: TestRelativeRelocationAppliesBaseAddress\n");
}

static void TestJmpSlotRelocationResolvesViaCallback() {
    uint32_t rel_relative_addr, rel_jmpslot_addr, rel_unresolved_addr;
    auto img = BuildSyntheticElf(rel_relative_addr, rel_jmpslot_addr, rel_unresolved_addr);

    ElfLoader loader;
    auto result = loader.Load(img.data(), img.size(), FakeResolver, nullptr);
    assert(result.has_value());

    uint8_t* p = result->AddressToPointer(rel_jmpslot_addr);
    assert(p != nullptr);
    uint32_t value;
    std::memcpy(&value, p, 4);
    assert(value == 0xDEAD0000u); // lo que devuelve FakeResolver para "resolved_symbol"

    std::printf("OK: TestJmpSlotRelocationResolvesViaCallback\n");
}

static void TestUnresolvedSymbolIsReportedNotIgnored() {
    uint32_t rel_relative_addr, rel_jmpslot_addr, rel_unresolved_addr;
    auto img = BuildSyntheticElf(rel_relative_addr, rel_jmpslot_addr, rel_unresolved_addr);

    ElfLoader loader;
    auto result = loader.Load(img.data(), img.size(), FakeResolver, nullptr);
    assert(result.has_value());

    assert(result->unresolved_symbols.size() == 1);
    assert(result->unresolved_symbols[0].name == "missing_symbol");
    assert(result->unresolved_symbols[0].got_or_plt_address == rel_unresolved_addr);

    std::printf("OK: TestUnresolvedSymbolIsReportedNotIgnored\n");
}

static void TestRejectsBadMagic() {
    std::vector<uint8_t> img(64, 0);
    ElfLoader loader;
    auto result = loader.Load(img.data(), img.size());
    assert(!result.has_value());
    std::printf("OK: TestRejectsBadMagic\n");
}

void RunElfLoaderTests() {
    TestValidElfParsesAndLoadsSegment();
    TestRelativeRelocationAppliesBaseAddress();
    TestJmpSlotRelocationResolvesViaCallback();
    TestUnresolvedSymbolIsReportedNotIgnored();
    TestRejectsBadMagic();
    std::printf("Todos los tests de elf_loader pasaron.\n");
}
