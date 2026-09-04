#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Filesystem virtual por-juego (docs/INPUT_OUTPUT.md, "Filesystem"): todo
// acceso a fichero del binario emulado se resuelve relativo a la raiz
// declarada en GameProfile.filesystem.root (p.ej. "sdmc:/arcade/hotd4/"),
// nunca a una ruta absoluta real del sistema. Mismo principio de
// aislamiento que usa lindbergh-loader (cada juego corre desde su propio
// directorio).
//
// Tambien es responsable de intercepciones tipo "ruta de dispositivo"
// (docs/ARCADE_HARDWARE.md: /dev/lbb, /dev/i2c/0, /dev/ttyS0, /dev/ttyS1
// en el binario original de Lindbergh) -- estas NO se resuelven contra el
// filesystem real de Horizon en absoluto, se redirigen a los shims de
// src/arcade/devices/.

namespace pas::os {

class VirtualFilesystem {
public:
    explicit VirtualFilesystem(std::string root);

    void RegisterDevicePath(const std::string& guest_path); // p.ej. "/dev/lbb"
    bool IsDevicePath(const std::string& guest_path) const;

    // Traduce una ruta tal y como la pide el binario emulado a una ruta
    // real bajo 'root_'. No aplica si IsDevicePath() es true -- esas se
    // resuelven en src/arcade/devices/, no aqui.
    std::string ResolveRealPath(const std::string& guest_path) const;

private:
    std::string root_;
    std::vector<std::string> device_paths_;
};

} // namespace pas::os
