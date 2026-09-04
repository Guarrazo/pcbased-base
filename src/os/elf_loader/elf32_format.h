#pragma once
#include <cstdint>

// Estructuras ELF32 tal y como están definidas por el formato público
// (System V ABI / gABI) -- no específicas de ningún binario concreto, así
// que no hay problema de "reproducir contenido de terceros" al declararlas:
// es un formato de fichero estándar, igual que declarar la cabecera de un
// PNG o un ZIP. Ver docs/CPU_TRANSLATION.md para el alcance real (x86 de
// 32 bits, ELF32, little-endian).

namespace pas::os::elf32 {

constexpr uint8_t kMagic[4] = {0x7F, 'E', 'L', 'F'};
constexpr uint8_t kClass32 = 1;
constexpr uint8_t kDataLsb = 1;
constexpr uint16_t kTypeExec = 2;
constexpr uint16_t kTypeDyn = 3;   // PIE o ET_DYN -- Lindbergh no usa PIE
                                    // segun la investigacion (docs/STATE_OF_THE_ART.md),
                                    // pero se soporta el tipo por si acaso
constexpr uint16_t kMachine386 = 3;

#pragma pack(push, 1)
struct Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

constexpr uint32_t kPtLoad = 1;
constexpr uint32_t kPtDynamic = 2;

struct Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

constexpr uint32_t kShtDynsym = 11;
constexpr uint32_t kShtDynamic = 6;
constexpr uint32_t kShtRel = 9;

struct Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

struct Sym {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
};

struct Rel {
    uint32_t r_offset;
    uint32_t r_info;
};

inline uint32_t RelType(uint32_t r_info) { return r_info & 0xFF; }
inline uint32_t RelSymIndex(uint32_t r_info) { return r_info >> 8; }

// Tipos de reubicacion x86 (R_386_*) realmente relevantes para binarios
// dinamicos de esta epoca -- subconjunto pequeno a proposito (docs/CPU_TRANSLATION.md,
// "no se pre-puebla de forma especulativa").
constexpr uint32_t kR386None     = 0;
constexpr uint32_t kR386_32      = 1;  // S + A
constexpr uint32_t kR386PC32     = 2;  // S + A - P
constexpr uint32_t kR386GlobDat  = 6;  // S
constexpr uint32_t kR386JmpSlot  = 7;  // S
constexpr uint32_t kR386Relative = 8;  // B + A

#pragma pack(pop)

} // namespace pas::os::elf32
