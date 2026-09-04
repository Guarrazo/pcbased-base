#include "cpu/jit/jit.h"
#include "cpu/jit/x86_cpu_state.h"
#include "cpu/arm64/emitter.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <memory>
#include <cstdlib>

#if defined(PAS_ARM64_HARNESS) && defined(PAS_QEMU_AARCH64)
#include <fstream>
#include <unistd.h>
#define PAS_HAVE_JIT_DYNEXEC 1
#endif

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

static void TestRunFromTranslatesAndCachesBlock() {
    uint8_t guest_memory[] = {0x83, 0xc0, 0x03, 0x83, 0xe0, 0x0f, 0xc3};

    auto mem = std::make_unique<FakeExecutableMemory>(4096);
    pas::cpu::jit::Jit jit(std::move(mem), guest_memory, 0, sizeof(guest_memory));

    assert(jit.FindTranslatedBlock(0) == nullptr);

    pas::cpu::jit::X86CpuState state{};
    state.regs[0] = 10;
    jit.RunFrom(0, state);

    const auto* block = jit.FindTranslatedBlock(0);
    assert(block != nullptr);
    assert(block->guest_address == 0);
    assert(block->code_size > 0);
    assert(block->code_size % 4 == 0);

    uint8_t* code = jit.GetExecutableCode(*block);
    assert(code != nullptr);

    std::printf("OK: TestRunFromTranslatesAndCachesBlock (%zu bytes ARM64)\n",
                block->code_size);

#ifdef PAS_HAVE_JIT_DYNEXEC
    assert(state.regs[0] == 13);
    std::printf("OK: Estado verificado: EAX=%u (esperado 13)\n", state.regs[0]);
#else
    std::printf("SKIP: verificacion dinamica\n");
#endif
}

static void TestRunFromReusesTranslatedBlock() {
    uint8_t guest_memory[] = {0x83, 0xc0, 0x03, 0x83, 0xe0, 0x0f, 0xc3};
    auto mem = std::make_unique<FakeExecutableMemory>(4096);
    pas::cpu::jit::Jit jit(std::move(mem), guest_memory, 0, sizeof(guest_memory));

    pas::cpu::jit::X86CpuState state1{};
    state1.regs[0] = 10;
    jit.RunFrom(0, state1);
    const auto* first = jit.FindTranslatedBlock(0);
    assert(first != nullptr);
    size_t offset_before = first->code_offset;

    pas::cpu::jit::X86CpuState state2{};
    state2.regs[0] = 20;
    jit.RunFrom(0, state2);
    const auto* second = jit.FindTranslatedBlock(0);
    assert(second != nullptr);
    assert(second->code_offset == offset_before);
    assert(state2.regs[0] == 23);

    std::printf("OK: TestRunFromReusesTranslatedBlock\n");
}

void RunJitIntegrationTests() {
    TestRunFromTranslatesAndCachesBlock();
    TestRunFromReusesTranslatedBlock();
    std::printf("Todos los tests de jit (integracion) pasaron.\n");
}
