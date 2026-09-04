#include "cpu/jit/code_cache.h"
#include "core/log.h"
#include <cstring>

namespace pas::cpu::jit {

CodeCache::CodeCache(std::unique_ptr<IExecutableMemory> memory)
    : memory_(std::move(memory)) {}

const CachedBlock* CodeCache::Find(uint32_t guest_address) const {
    auto it = blocks_.find(guest_address);
    return it != blocks_.end() ? &it->second : nullptr;
}

const CachedBlock* CodeCache::Insert(uint32_t guest_address, const uint8_t* code, size_t code_size) {
    if (!memory_) {
        PAS_LOG_ERROR("CodeCache", "Insert sin memoria ejecutable configurada");
        return nullptr;
    }
    if (write_cursor_ + code_size > memory_->Capacity()) {
        PAS_LOG_ERROR("CodeCache", "Sin espacio para nuevo bloque (addr=0x%08x, size=%zu)",
                      guest_address, code_size);
        return nullptr;
    }

    uint8_t* rw = memory_->BeginWrite();
    if (!rw) {
        PAS_LOG_ERROR("CodeCache", "BeginWrite() devolvio nullptr (addr=0x%08x)", guest_address);
        return nullptr;
    }
    std::memcpy(rw + write_cursor_, code, code_size);
    memory_->EndWrite();

    CachedBlock block;
    block.guest_address = guest_address;
    block.code_offset = write_cursor_;
    block.code_size = code_size;
    write_cursor_ += code_size;

    auto [it, inserted] = blocks_.insert_or_assign(guest_address, block);
    PAS_LOG_INFO("CodeCache", "Bloque insertado: guest=0x%08x, offset=%zu, size=%zu",
                guest_address, block.code_offset, code_size);
    return &it->second;
}

void CodeCache::InvalidateRange(uint32_t guest_address, size_t length) {
    // TODO(Nivel 2, docs/ROADMAP.md): recorrer blocks_ y eliminar los que
    // solapen con [guest_address, guest_address+length). No implementado
    // en este esqueleto -- se registra explicitamente para no fallar mudo.
    PAS_LOG_WARN("CodeCache", "InvalidateRange no implementado (addr=0x%08x, len=%zu)",
                 guest_address, length);
}

uint8_t* CodeCache::ExecutableAddress(const CachedBlock& block) const {
    if (!memory_) return nullptr;
    return memory_->ExecutableBase() + block.code_offset;
}

bool CodeCache::SaveToDisk(const char* path) const {
    PAS_LOG_WARN("CodeCache", "SaveToDisk no implementado (path=%s)", path);
    return false;
}

bool CodeCache::LoadFromDisk(const char* path) {
    PAS_LOG_WARN("CodeCache", "LoadFromDisk no implementado (path=%s)", path);
    return false;
}

} // namespace pas::cpu::jit
