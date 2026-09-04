#include "cpu/x86/decoder.h"
#include "cpu/translator/ir_builder.h"
#include "cpu/jit/arm64_codegen.h"
#include "cpu/arm64/emitter.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <unistd.h>

using namespace pas::cpu;

namespace {

#if defined(PAS_X86_HARNESS) && defined(PAS_ARM64_HARNESS) && defined(PAS_QEMU_AARCH64)
#define PAS_HAVE_DIFF_TEST_HARNESSES 1

const std::vector<uint8_t> kAddAndInstructions = {0x83, 0xc0, 0x03, 0x83, 0xe0, 0x0f};
const std::vector<uint8_t> kAddAndWithRet = {0x83, 0xc0, 0x03, 0x83, 0xe0, 0x0f, 0xc3};

std::string WriteTempFile(const std::vector<uint8_t>& bytes) {
    std::string tmpl = "/tmp/pas_diff_test_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    assert(fd >= 0);
    close(fd);
    std::string path(buf.data());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return path;
}

int RunViaHarness(const std::string& harness_cmd_prefix, const std::string& bin_path, int64_t input) {
    std::string cmd = harness_cmd_prefix + " " + bin_path + " " + std::to_string(input);
    FILE* pipe = popen(cmd.c_str(), "r");
    assert(pipe != nullptr);
    char line[256] = {0};
    char* got = std::fgets(line, sizeof(line), pipe);
    int rc = pclose(pipe);
    assert(got != nullptr);
    assert(rc == 0);
    return std::atoi(line);
}

std::vector<uint8_t> TranslateToArm64() {
    x86::Decoder decoder;
    std::vector<x86::X86Instruction> instructions;
    size_t offset = 0;
    uint32_t addr = 0;
    while (offset < kAddAndInstructions.size()) {
        x86::X86Instruction inst;
        bool ok = decoder.DecodeOne(kAddAndInstructions.data() + offset,
                                     kAddAndInstructions.size() - offset, addr, inst);
        assert(ok);
        instructions.push_back(inst);
        offset += inst.length;
        addr += inst.length;
    }
    assert(instructions.size() == 2);

    translator::IrBuilder ir_builder;
    ir::Block block = ir_builder.BuildBlock(instructions.data(), instructions.size());
    assert(block.instructions.size() > 0);

    std::vector<uint8_t> buf(256);
    arm64::Emitter emitter(buf.data(), buf.size());

    emitter.EmitOrrReg(jit::Arm64CodeGen::MappedRegister(0), 31, 0, false);

    jit::Arm64CodeGen codegen(nullptr, 0);
    bool ok = codegen.Generate(block, emitter);
    assert(ok);

    emitter.EmitOrrReg(0, 31, jit::Arm64CodeGen::MappedRegister(0), false);
    emitter.EmitRet();

    assert(!emitter.HadEncodingError());
    assert(!emitter.Overflowed());
    buf.resize(emitter.BytesWritten());
    return buf;
}

#endif

} // namespace

static void TestTranslatedArm64MatchesNativeX86ForSeveralInputs() {
#if PAS_HAVE_DIFF_TEST_HARNESSES
    std::vector<uint8_t> arm64_code = TranslateToArm64();
    std::string arm64_bin_path = WriteTempFile(arm64_code);
    std::string x86_bin_path = WriteTempFile(kAddAndWithRet);

    std::string x86_cmd = PAS_X86_HARNESS;
    std::string arm64_cmd = std::string(PAS_QEMU_AARCH64) + " " + PAS_ARM64_HARNESS;

    const int test_inputs[] = {0, 1, 5, 10, -1, -5, 100, 255, -100, 2147483647, -2147483648};

    for (int input : test_inputs) {
        int native_result = RunViaHarness(x86_cmd, x86_bin_path, input);
        int translated_result = RunViaHarness(arm64_cmd, arm64_bin_path, input);
        if (native_result != translated_result) {
            std::printf("DISCREPANCIA para entrada %d: nativo x86=%d, traducido ARM64=%d\n",
                        input, native_result, translated_result);
        }
        assert(native_result == translated_result);
    }

    std::remove(arm64_bin_path.c_str());
    std::remove(x86_bin_path.c_str());

    std::printf("OK: TestTranslatedArm64MatchesNativeX86ForSeveralInputs (%zu valores)\n",
                std::size(test_inputs));
#else
    std::printf("SKIP: TestTranslatedArm64MatchesNativeX86ForSeveralInputs\n");
#endif
}

void RunIrEndToEndTests() {
    TestTranslatedArm64MatchesNativeX86ForSeveralInputs();
    std::printf("Todos los tests de ir_end_to_end pasaron.\n");
}
