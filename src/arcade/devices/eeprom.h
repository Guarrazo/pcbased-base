#pragma once
#include <cstdint>
#include <vector>
#include <string>

// Síntesis de eeprom.bin/sram.bin (docs/ARCADE_HARDWARE.md, docs/STATE_OF_THE_ART.md):
// [CONFIRMADO por lindbergh-loader] no existe volcado real de keychip -- la
// "placa de seguridad" de Lindbergh son DIP switches y líneas test/service.
// eeprom.bin se sintetiza calculando los CRC de cada sección
// amSysDataRecord a partir de constantes conocidas; sram.bin se crea vacío
// en el primer arranque.
//
// [DESCONOCIDO]: el algoritmo exacto de CRC y el layout byte a byte de
// amSysDataRecord no se han re-derivado en esta pasada de investigación --
// hay que tomarlos de la documentación/código de lindbergh-loader (proyecto
// GPL, revisar términos de licencia antes de portar código literal; el
// FORMATO en sí, al ser resultado de ingeniería inversa de un formato de
// datos, es lo reutilizable con más seguridad, no necesariamente el código).

namespace pas::arcade {

class EepromSynthesizer {
public:
    // Genera el contenido de eeprom.bin para el PlatformProfile "lindbergh".
    // Devuelve vacío si el layout aún no está implementado (ver TODO en el
    // .cpp) -- el llamador debe tratar un vector vacío como fallo explícito.
    std::vector<uint8_t> SynthesizeEeprom();

    std::vector<uint8_t> SynthesizeSram();

    // Persiste a la raíz de filesystem del GameProfile activo (docs/GAME_PROFILES.md)
    // si no existen ya -- igual que lindbergh-loader, que solo sintetiza en
    // el primer arranque y reutiliza el fichero en arranques posteriores.
    bool WriteIfMissing(const std::string& eeprom_path, const std::string& sram_path);
};

} // namespace pas::arcade
