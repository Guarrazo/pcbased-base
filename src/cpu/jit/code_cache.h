#pragma once
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <memory>

// Gestion de memoria ejecutable + cache de bloques traducidos.
//
// La asignacion de memoria RW/RX real es especifica de Horizon OS
// (jitCreate/jitTransitionToWritable/jitTransitionToExecutable, ver
// docs/SWITCH_PLATFORM.md y docs/JIT.md) -- por eso esta interfaz es
// abstracta aqui (independiente de plataforma) y la implementa
// platform/switch/jit_memory.cpp, que es el UNICO fichero de todo el
// proyecto que debe llamar a jitCreate directamente.

namespace pas::cpu::jit {

// Implementado en platform/switch/jit_memory.cpp
class IExecutableMemory {
public:
    virtual ~IExecutableMemory() = default;
    virtual uint8_t* BeginWrite() = 0;   // vista RW de la region
    virtual void EndWrite() = 0;         // transiciona a RX (jitTransitionToExecutable)
    virtual uint8_t* ExecutableBase() = 0; // puntero RX real, para saltar a el
    virtual size_t Capacity() const = 0;
};

struct CachedBlock {
    uint32_t guest_address = 0;
    size_t code_offset = 0;   // offset dentro de IExecutableMemory
    size_t code_size = 0;
};

class CodeCache {
public:
    explicit CodeCache(std::unique_ptr<IExecutableMemory> memory);

    // nullptr si el bloque para esta direccion x86 no esta traducido todavia.
    const CachedBlock* Find(uint32_t guest_address) const;

    // Reserva espacio en la region ejecutable, COPIA 'code' dentro (via
    // memory->BeginWrite()/EndWrite(), encapsulado aqui -- el llamador
    // nunca toca IExecutableMemory directamente) y registra el bloque.
    // Devuelve nullptr si no hay espacio o si memory_ es nulo.
    const CachedBlock* Insert(uint32_t guest_address, const uint8_t* code, size_t code_size);

    // Invalidacion por self-modifying code (docs/JIT.md) -- elimina
    // cualquier bloque cuyo rango de bytes-fuente x86 se solape con
    // [addr, addr+len). NO implementado en este esqueleto (Nivel 2 del
    // MVP, ver docs/ROADMAP.md) pero la interfaz esta desde el principio
    // por la leccion ya aprendida en Super3-NX (PpcJitInvalidate() sin
    // call site fue marcado P0 alli).
    void InvalidateRange(uint32_t guest_address, size_t length);

    // Puntero ejecutable (RX) real al codigo de un bloque ya insertado --
    // usado por el dispatcher (Jit::RunFrom) para saltar a el. nullptr si
    // memory_ es nulo.
    uint8_t* ExecutableAddress(const CachedBlock& block) const;

    // Serializacion a sdmc:/ entre ejecuciones (docs/JIT.md, "Cache de
    // codigo persistente"). No implementado en este esqueleto.
    bool SaveToDisk(const char* path) const;
    bool LoadFromDisk(const char* path);

private:
    std::unique_ptr<IExecutableMemory> memory_;
    std::unordered_map<uint32_t, CachedBlock> blocks_;
    size_t write_cursor_ = 0;
};

} // namespace pas::cpu::jit
