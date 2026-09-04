#include "arcade/devices/eeprom.h"
#include "core/log.h"
#include <fstream>

namespace pas::arcade {

std::vector<uint8_t> EepromSynthesizer::SynthesizeEeprom() {
    // TODO(Nivel 1, docs/ROADMAP.md): layout real de amSysDataRecord + CRC.
    // No implementado en este esqueleto -- ver docs/ARCADE_HARDWARE.md.
    PAS_LOG_ERROR("EepromSynthesizer", "SynthesizeEeprom no implementado todavia");
    return {};
}

std::vector<uint8_t> EepromSynthesizer::SynthesizeSram() {
    // sram.bin se crea vacio segun lindbergh-loader -- esto SI es trivial
    // de reproducir ya (tamano real del bloque aun por confirmar contra un
    // binario real, ver docs/ROADMAP.md "no se sabe hasta medir").
    PAS_LOG_WARN("EepromSynthesizer", "SynthesizeSram: tamano real del bloque sram no confirmado todavia");
    return {};
}

bool EepromSynthesizer::WriteIfMissing(const std::string& eeprom_path, const std::string& sram_path) {
    std::ifstream existing_eeprom(eeprom_path, std::ios::binary);
    std::ifstream existing_sram(sram_path, std::ios::binary);
    if (existing_eeprom.good() && existing_sram.good()) {
        PAS_LOG_INFO("EepromSynthesizer", "eeprom.bin/sram.bin ya existen, no se regeneran");
        return true;
    }

    auto eeprom = SynthesizeEeprom();
    auto sram = SynthesizeSram();
    if (eeprom.empty() || sram.empty()) {
        PAS_LOG_ERROR("EepromSynthesizer", "Sintesis incompleta, no se escribe nada a disco");
        return false;
    }
    // TODO: escritura real a disco -- no implementado en este esqueleto.
    return false;
}

} // namespace pas::arcade
