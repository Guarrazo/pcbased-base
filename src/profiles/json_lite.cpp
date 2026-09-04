#include "profiles/json_lite.h"
#include <cctype>
#include <sstream>

namespace pas::profiles::json {

namespace {

const std::vector<Value> kEmptyArray;

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    bool ParseDocument(Value& out, std::string& error) {
        SkipWhitespace();
        if (!ParseValue(out, error)) return false;
        SkipWhitespace();
        if (pos_ != text_.size()) {
            error = "Contenido extra tras el JSON valido en offset " + std::to_string(pos_);
            return false;
        }
        return true;
    }

private:
    const std::string& text_;
    size_t pos_ = 0;

    bool AtEnd() const { return pos_ >= text_.size(); }
    char Peek() const { return AtEnd() ? '\0' : text_[pos_]; }

    void SkipWhitespace() {
        while (!AtEnd()) {
            char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else if (c == '/' && pos_ + 1 < text_.size() && text_[pos_ + 1] == '/') {
                // Comentarios de linea // -- no es JSON estandar, pero
                // conveniente para perfiles editados a mano (ver
                // docs/GAME_PROFILES.md, los ejemplos usan JSONC).
                while (!AtEnd() && text_[pos_] != '\n') ++pos_;
            } else {
                break;
            }
        }
    }

    bool Expect(char c, std::string& error) {
        if (AtEnd() || text_[pos_] != c) {
            error = std::string("Se esperaba '") + c + "' en offset " + std::to_string(pos_);
            return false;
        }
        ++pos_;
        return true;
    }

    bool ParseValue(Value& out, std::string& error) {
        SkipWhitespace();
        if (AtEnd()) { error = "Fin de fichero inesperado"; return false; }
        char c = Peek();
        if (c == '{') return ParseObject(out, error);
        if (c == '[') return ParseArray(out, error);
        if (c == '"') return ParseString(out, error);
        if (c == 't' || c == 'f') return ParseBool(out, error);
        if (c == 'n') return ParseNull(out, error);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber(out, error);
        error = std::string("Token inesperado '") + c + "' en offset " + std::to_string(pos_);
        return false;
    }

    bool ParseObject(Value& out, std::string& error) {
        if (!Expect('{', error)) return false;
        std::map<std::string, Value> fields;
        SkipWhitespace();
        if (Peek() == '}') { ++pos_; out = Value::MakeObject(std::move(fields)); return true; }

        while (true) {
            SkipWhitespace();
            Value key;
            if (!ParseString(key, error)) return false;
            SkipWhitespace();
            if (!Expect(':', error)) return false;
            Value val;
            if (!ParseValue(val, error)) return false;
            fields[key.AsString()] = std::move(val);

            SkipWhitespace();
            if (Peek() == ',') { ++pos_; continue; }
            if (Peek() == '}') { ++pos_; break; }
            error = "Se esperaba ',' o '}' en objeto, offset " + std::to_string(pos_);
            return false;
        }
        out = Value::MakeObject(std::move(fields));
        return true;
    }

    bool ParseArray(Value& out, std::string& error) {
        if (!Expect('[', error)) return false;
        std::vector<Value> items;
        SkipWhitespace();
        if (Peek() == ']') { ++pos_; out = Value::MakeArray(std::move(items)); return true; }

        while (true) {
            Value val;
            if (!ParseValue(val, error)) return false;
            items.push_back(std::move(val));

            SkipWhitespace();
            if (Peek() == ',') { ++pos_; continue; }
            if (Peek() == ']') { ++pos_; break; }
            error = "Se esperaba ',' o ']' en array, offset " + std::to_string(pos_);
            return false;
        }
        out = Value::MakeArray(std::move(items));
        return true;
    }

    // Codifica un code point Unicode <= 0xFFFF como UTF-8 (no se soportan
    // pares suplentes / codepoints > 0xFFFF -- suficiente para el uso
    // previsto de nombres de perfil, ver el comentario de json_lite.h).
    static void AppendUtf8(std::string& out, uint32_t codepoint) {
        if (codepoint <= 0x7F) {
            out += static_cast<char>(codepoint);
        } else if (codepoint <= 0x7FF) {
            out += static_cast<char>(0xC0 | (codepoint >> 6));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (codepoint >> 12));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }

    bool ParseString(Value& out, std::string& error) {
        if (!Expect('"', error)) return false;
        std::string result;
        while (true) {
            if (AtEnd()) { error = "Cadena sin cerrar"; return false; }
            char c = text_[pos_++];
            if (c == '"') break;
            if (c == '\\') {
                if (AtEnd()) { error = "Escape incompleto al final del fichero"; return false; }
                char esc = text_[pos_++];
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) { error = "\\u incompleto"; return false; }
                        uint32_t cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = text_[pos_++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                            else { error = "Digito hex invalido en \\u"; return false; }
                        }
                        AppendUtf8(result, cp);
                        break;
                    }
                    default:
                        error = std::string("Escape desconocido '\\") + esc + "'";
                        return false;
                }
            } else {
                result += c;
            }
        }
        out = Value::MakeString(std::move(result));
        return true;
    }

    bool ParseBool(Value& out, std::string& error) {
        if (text_.compare(pos_, 4, "true") == 0) { pos_ += 4; out = Value::MakeBool(true); return true; }
        if (text_.compare(pos_, 5, "false") == 0) { pos_ += 5; out = Value::MakeBool(false); return true; }
        error = "Literal booleano invalido en offset " + std::to_string(pos_);
        return false;
    }

    bool ParseNull(Value& out, std::string& error) {
        if (text_.compare(pos_, 4, "null") == 0) { pos_ += 4; out = Value::MakeNull(); return true; }
        error = "Literal invalido (se esperaba 'null') en offset " + std::to_string(pos_);
        return false;
    }

    bool ParseNumber(Value& out, std::string& error) {
        size_t start = pos_;
        if (Peek() == '-') ++pos_;
        while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) ++pos_;
        if (Peek() == '.') {
            ++pos_;
            while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) ++pos_;
        }
        if (Peek() == 'e' || Peek() == 'E') {
            ++pos_;
            if (Peek() == '+' || Peek() == '-') ++pos_;
            while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) ++pos_;
        }
        if (pos_ == start) { error = "Numero invalido en offset " + std::to_string(pos_); return false; }
        try {
            out = Value::MakeNumber(std::stod(text_.substr(start, pos_ - start)));
        } catch (...) {
            error = "No se pudo convertir el numero en offset " + std::to_string(start);
            return false;
        }
        return true;
    }
};

} // namespace

bool Value::AsBool(bool fallback) const { return type_ == Type::Bool ? bool_ : fallback; }
double Value::AsNumber(double fallback) const { return type_ == Type::Number ? number_ : fallback; }
int Value::AsInt(int fallback) const { return type_ == Type::Number ? static_cast<int>(number_) : fallback; }
std::string Value::AsString(const std::string& fallback) const {
    return type_ == Type::String ? string_ : fallback;
}

const Value* Value::Get(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    auto it = object_.find(key);
    return it != object_.end() ? &it->second : nullptr;
}

const std::vector<Value>& Value::AsArray() const {
    return type_ == Type::Array ? array_ : kEmptyArray;
}

Value Value::MakeNull() { return Value(); }

Value Value::MakeBool(bool b) {
    Value v; v.type_ = Type::Bool; v.bool_ = b; return v;
}
Value Value::MakeNumber(double n) {
    Value v; v.type_ = Type::Number; v.number_ = n; return v;
}
Value Value::MakeString(std::string s) {
    Value v; v.type_ = Type::String; v.string_ = std::move(s); return v;
}
Value Value::MakeArray(std::vector<Value> items) {
    Value v; v.type_ = Type::Array; v.array_ = std::move(items); return v;
}
Value Value::MakeObject(std::map<std::string, Value> fields) {
    Value v; v.type_ = Type::Object; v.object_ = std::move(fields); return v;
}

bool Parse(const std::string& text, Value& out, std::string& error_message) {
    Parser parser(text);
    return parser.ParseDocument(out, error_message);
}

} // namespace pas::profiles::json
