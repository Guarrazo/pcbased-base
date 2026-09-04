// Test de host para patch::PatchEngine -- ver docs/PATCHING.md.
// Sin dependencia de framework externo (assert-based) a propósito, para
// mantener el esqueleto sin dependencias de terceros hasta que haga falta
// algo más elaborado (ver third_party/README.md).
#include "patch/patch_engine.h"
#include <cassert>
#include <cstdio>
#include <vector>

static void TestByteToBytePatchAppliesWhenExpectMatches() {
    uint8_t memory[4] = {0x74, 0x02, 0x00, 0x00}; // JZ +2, como en el ejemplo de docs/PATCHING.md

    pas::patch::PatchEngine engine;
    pas::patch::BytePatch patch;
    patch.offset = 0;
    patch.expect = {0x74, 0x02};
    patch.replace = {0x90, 0x90}; // NOP NOP
    engine.LoadBytePatches({patch});

    bool ok = engine.ApplyBytePatches(memory, sizeof(memory));
    assert(ok);
    assert(memory[0] == 0x90 && memory[1] == 0x90);
    std::printf("OK: TestByteToBytePatchAppliesWhenExpectMatches\n");
}

static void TestBytePatchRejectedWhenExpectDoesNotMatch() {
    uint8_t memory[4] = {0xAA, 0xBB, 0x00, 0x00}; // no coincide con 'expect'

    pas::patch::PatchEngine engine;
    pas::patch::BytePatch patch;
    patch.offset = 0;
    patch.expect = {0x74, 0x02};
    patch.replace = {0x90, 0x90};
    engine.LoadBytePatches({patch});

    bool ok = engine.ApplyBytePatches(memory, sizeof(memory));
    assert(!ok); // debe rechazar el patch, no aplicarlo a ciegas (docs/PATCHING.md)
    assert(memory[0] == 0xAA && memory[1] == 0xBB); // memoria intacta
    std::printf("OK: TestBytePatchRejectedWhenExpectDoesNotMatch\n");
}

static void TestBytePatchRejectedWhenOutOfRange() {
    uint8_t memory[2] = {0x00, 0x00};

    pas::patch::PatchEngine engine;
    pas::patch::BytePatch patch;
    patch.offset = 10; // fuera de rango de un buffer de 2 bytes
    patch.expect = {0x74, 0x02};
    patch.replace = {0x90, 0x90};
    engine.LoadBytePatches({patch});

    bool ok = engine.ApplyBytePatches(memory, sizeof(memory));
    assert(!ok);
    std::printf("OK: TestBytePatchRejectedWhenOutOfRange\n");
}

static void TestHasHookForSymbol() {
    pas::patch::PatchEngine engine;
    pas::patch::SymbolHook hook;
    hook.symbol = "check_security_dongle";
    hook.action = pas::patch::SymbolHook::Action::StubReturnOk;
    engine.LoadSymbolHooks({hook});

    assert(engine.HasHookForSymbol("check_security_dongle"));
    assert(!engine.HasHookForSymbol("otra_funcion"));
    std::printf("OK: TestHasHookForSymbol\n");
}

void RunPatchEngineTests() {
    TestByteToBytePatchAppliesWhenExpectMatches();
    TestBytePatchRejectedWhenExpectDoesNotMatch();
    TestBytePatchRejectedWhenOutOfRange();
    TestHasHookForSymbol();
    std::printf("Todos los tests de patch_engine pasaron.\n");
}
