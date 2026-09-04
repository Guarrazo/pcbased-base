#include "cpu/jit/jit.h"
#include "cpu/jit/x86_cpu_state.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <memory>
#include <cstring>

class FakeExecutableMemory : public pas::cpu::jit::IExecutableMemory {
public:
    explicit FakeExecutableMemory(size_t s) : buf_(s) {}
    uint8_t* BeginWrite() override { return buf_.data(); }
    void EndWrite() override {}
    uint8_t* ExecutableBase() override { return buf_.data(); }
    size_t Capacity() const override { return buf_.size(); }
private:
    std::vector<uint8_t> buf_;
};

void RunPhase1MemoryStackTests() {
    uint8_t code[] = {
        0x55,
        0x89, 0xE5,
        0x83, 0xEC, 0x04,
        0xC7, 0x45, 0xFC, 0x2A, 0x00, 0x00, 0x00,
        0x8B, 0x45, 0xFC,
        0x89, 0xEC,
        0x5D,
        0xC3
    };

    std::vector<uint8_t> guest_mem(65536, 0);
    std::memcpy(guest_mem.data(), code, sizeof(code));
    uint32_t stack_top = 0x10000;

    pas::cpu::jit::X86CpuState state{};
    state.regs[4] = stack_top;
    state.regs[5] = 0xDEADBEEF;

    auto mem = std::make_unique<FakeExecutableMemory>(65536);
    pas::cpu::jit::Jit jit(std::move(mem), guest_mem.data(), 0,
                           static_cast<uint32_t>(guest_mem.size()));

    jit.RunFrom(0, state);

    assert(state.regs[0] == 42);
    assert(state.regs[4] == stack_top);
    assert(state.regs[5] == 0xDEADBEEF);

    std::printf("OK: Phase1MemoryStackTests -- EAX=%u, ESP=0x%x, EBP=0x%x\n",
                state.regs[0], state.regs[4], state.regs[5]);
}
