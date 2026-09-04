#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <cstdint>

// Parser JSON mínimo, hecho a medida para el esquema documentado en
// docs/GAME_PROFILES.md -- NO es un parser JSON de propósito general
// (no cubre unicode \uXXXX, no valida exhaustivamente, no optimiza).
//
// Por qué esto y no una librería de terceros (ver third_party/README.md):
// el entorno donde se generó este esqueleto no tenía acceso de red para
// vendorizar nlohmann/json con la versión correcta. Este parser es
// deliberadamente pequeño (un fichero, ~150 líneas) y con tests (ver
// tests/test_json_lite.cpp) para que sea reemplazable sin drama en cuanto
// vendorices una librería real -- toda la superficie que toca el resto del
// código es JsonValue::AsString()/AsInt()/etc., no el parser en sí.

namespace pas::profiles::json {

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
    Value() : type_(Type::Null) {}

    Type type() const { return type_; }
    bool IsNull() const { return type_ == Type::Null; }

    // Devuelven el valor por defecto dado si el tipo no coincide, en vez de
    // lanzar -- el llamador (game_profile.cpp) decide si un campo ausente
    // es un error o un default razonable, el parser no lo decide por él.
    bool AsBool(bool fallback = false) const;
    double AsNumber(double fallback = 0.0) const;
    int AsInt(int fallback = 0) const;
    std::string AsString(const std::string& fallback = "") const;

    // nullptr si no es objeto o la clave no existe.
    const Value* Get(const std::string& key) const;
    // vector vacio si no es array.
    const std::vector<Value>& AsArray() const;

    static Value MakeNull();
    static Value MakeBool(bool b);
    static Value MakeNumber(double n);
    static Value MakeString(std::string s);
    static Value MakeArray(std::vector<Value> items);
    static Value MakeObject(std::map<std::string, Value> fields);

private:
    Type type_;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Value> array_;
    std::map<std::string, Value> object_;
};

// Parsea 'text' completo. Devuelve false (sin modificar 'out') si el JSON
// es invalido -- 'error_message' se rellena con una descripcion breve
// (offset + que se esperaba) para que aparezca en el log del llamador.
bool Parse(const std::string& text, Value& out, std::string& error_message);

} // namespace pas::profiles::json
