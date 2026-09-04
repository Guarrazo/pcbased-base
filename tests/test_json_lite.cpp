// Test de host para profiles::json -- ver docs/GAME_PROFILES.md y el
// comentario en src/profiles/json_lite.h sobre por que existe este parser
// a medida en vez de una libreria de terceros.
#include "profiles/json_lite.h"
#include <cassert>
#include <cstdio>

using namespace pas::profiles::json;

static void TestParsesFlatObject() {
    Value root;
    std::string error;
    bool ok = Parse(R"({"id": "hotd4", "revision": 3, "enabled": true, "ratio": 1.5})",
                     root, error);
    assert(ok);
    assert(root.Get("id")->AsString() == "hotd4");
    assert(root.Get("revision")->AsInt() == 3);
    assert(root.Get("enabled")->AsBool() == true);
    assert(root.Get("ratio")->AsNumber() > 1.4 && root.Get("ratio")->AsNumber() < 1.6);
    std::printf("OK: TestParsesFlatObject\n");
}

static void TestParsesNestedObjectsAndArrays() {
    Value root;
    std::string error;
    bool ok = Parse(R"({
        "display": { "internal_resolution": [1280, 720], "aspect": "4:3" },
        "required_apis": ["libc_shim", "cg_3_1", "gl_1_x"]
    })", root, error);
    assert(ok);

    auto* display = root.Get("display");
    assert(display != nullptr);
    auto* res = display->Get("internal_resolution");
    assert(res->AsArray().size() == 2);
    assert(res->AsArray()[0].AsInt() == 1280);
    assert(res->AsArray()[1].AsInt() == 720);
    assert(display->Get("aspect")->AsString() == "4:3");

    auto* apis = root.Get("required_apis");
    assert(apis->AsArray().size() == 3);
    assert(apis->AsArray()[1].AsString() == "cg_3_1");

    std::printf("OK: TestParsesNestedObjectsAndArrays\n");
}

static void TestParsesEscapesAndComments() {
    Value root;
    std::string error;
    // Incluye un comentario de linea (extension JSONC, ver json_lite.h) y
    // un escape \" dentro de la cadena.
    bool ok = Parse(R"({
        // comentario de prueba
        "name": "The \"House\" of the Dead"
    })", root, error);
    assert(ok);
    assert(root.Get("name")->AsString() == "The \"House\" of the Dead");
    std::printf("OK: TestParsesEscapesAndComments\n");
}

static void TestRejectsInvalidJson() {
    Value root;
    std::string error;
    bool ok = Parse(R"({"id": "hotd4",})", root, error); // coma colgante -> invalido
    assert(!ok);
    assert(!error.empty());
    std::printf("OK: TestRejectsInvalidJson (error: %s)\n", error.c_str());
}

static void TestMissingKeyReturnsNullptr() {
    Value root;
    std::string error;
    bool ok = Parse(R"({"id": "hotd4"})", root, error);
    assert(ok);
    assert(root.Get("does_not_exist") == nullptr);
    std::printf("OK: TestMissingKeyReturnsNullptr\n");
}

void RunJsonLiteTests() {
    TestParsesFlatObject();
    TestParsesNestedObjectsAndArrays();
    TestParsesEscapesAndComments();
    TestRejectsInvalidJson();
    TestMissingKeyReturnsNullptr();
    std::printf("Todos los tests de json_lite pasaron.\n");
}
