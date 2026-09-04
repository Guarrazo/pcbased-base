// Test de host para cpu::jit::CodeCache -- usa un IExecutableMemory falso
// (sin jitCreate real, ver docs/JIT.md) porque este test corre en la
// máquina de desarrollo, no en Switch.
#include "cpu/jit/code_cache.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <memory>

namespace {

class FakeExecutableMemory : public pas::cpu::jit::IExecutableMemory {
public:
    explicit FakeExecutableMemory(size_t size) : buffer_(size, 0) {}
    uint8_t* BeginWrite() override { return buffer_.data(); }
    void EndWrite() override {}
    uint8_t* ExecutableBase() override { return buffer_.data(); }
    size_t Capacity() const override { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
};

} // namespace

static void TestInsertAndFind() {
    auto mem = std::make_unique<FakeExecutableMemory>(1024);
    pas::cpu::jit::CodeCache cache(std::move(mem));

    assert(cache.Find(0x1000) == nullptr); // nada traducido todavia

    std::vector<uint8_t> code(64, 0xAB);
    const auto* block = cache.Insert(0x1000, code.data(), code.size());
    assert(block != nullptr);
    assert(block->guest_address == 0x1000);
    assert(block->code_size == 64);

    const auto* found = cache.Find(0x1000);
    assert(found != nullptr);
    assert(found->code_offset == block->code_offset);

    std::printf("OK: TestInsertAndFind\n");
}

static void TestInsertFailsWhenOutOfSpace() {
    auto mem = std::make_unique<FakeExecutableMemory>(16); // muy pequeño a proposito
    pas::cpu::jit::CodeCache cache(std::move(mem));

    std::vector<uint8_t> code(64, 0xAB); // no cabe
    const auto* block = cache.Insert(0x2000, code.data(), code.size());
    assert(block == nullptr);

    std::printf("OK: TestInsertFailsWhenOutOfSpace\n");
}

static void TestInsertActuallyCopiesBytes() {
    // A diferencia de la version anterior (que solo reservaba espacio),
    // Insert() ahora copia los bytes de verdad -- este test lo comprueba
    // explicitamente, no solo el tamano reservado.
    auto mem = std::make_unique<FakeExecutableMemory>(1024);
    auto* mem_ptr = mem.get(); // valido tras move() porque es solo el puntero, no el objeto
    pas::cpu::jit::CodeCache cache(std::move(mem));

    uint8_t code[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    const auto* block = cache.Insert(0x3000, code, sizeof(code));
    assert(block != nullptr);

    uint8_t* exec_ptr = cache.ExecutableAddress(*block);
    assert(exec_ptr != nullptr);
    assert(std::memcmp(exec_ptr, code, sizeof(code)) == 0);
    (void)mem_ptr;

    std::printf("OK: TestInsertActuallyCopiesBytes\n");
}

void RunCodeCacheTests() {
    TestInsertAndFind();
    TestInsertFailsWhenOutOfSpace();
    TestInsertActuallyCopiesBytes();
    std::printf("Todos los tests de code_cache pasaron.\n");
}
