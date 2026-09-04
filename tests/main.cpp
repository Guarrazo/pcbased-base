// Runner compartido de los tests de host -- ver docs/ROADMAP.md.
// Cada test_*.cpp expone una funcion Run*Tests() en vez de su propio main(),
// para poder enlazarlos todos en un unico ejecutable pas_tests.
#include <cstdio>

void RunPatchEngineTests();
void RunElfLoaderTests();
void RunCodeCacheTests();
void RunJsonLiteTests();
void RunGameProfileTests();
void RunArm64EmitterTests();
void RunElfLoaderRealBinaryTests();
void RunX86DecoderTests();
void RunArm64DynamicExecTests();
void RunIrEndToEndTests();
void RunJitIntegrationTests();
void RunPhase1MemoryStackTests();

int main() {
    std::printf("== Running all tests ==\n");
    RunPatchEngineTests();
    RunElfLoaderTests();
    RunCodeCacheTests();
    RunJsonLiteTests();
    RunGameProfileTests();
    RunArm64EmitterTests();
    RunElfLoaderRealBinaryTests();
    RunX86DecoderTests();
    RunArm64DynamicExecTests();
    RunIrEndToEndTests();
    RunJitIntegrationTests();
    RunPhase1MemoryStackTests();
    std::printf("== All tests passed ==\n");
    return 0;
}
