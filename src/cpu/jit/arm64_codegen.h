#pragma once
#include "cpu/translator/ir.h"
#include "cpu/arm64/emitter.h"
#include <unordered_map>

// Genera código ARM64 real a partir de un cpu::ir::Block, usando
// asignación de registros FIJA (no dinámica): cada registro x86 vive
// siempre en el mismo registro ARM64 mientras el JIT está corriendo, en
// vez de asignarse dinámicamente por bloque. Es la misma clase de decisión
// que "modo estático" de Box64 -- más simple y predecible que un
// asignador de registros de verdad, a costa de no poder mantener valores
// temporales en registro entre bloques (esto es aceptable para el MVP,
// ver docs/JIT.md sobre "nada de optimizaciones caras").
//
// Convención de registros (ver docs/JIT.md, sección "Convención de
// registros ARM64"):
//   W20-W27  <- EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI (orden ModRM, 0-7)
//   X28      <- "translation base" (host_ptr = X28 + guest_virtual_addr),
//               no usado todavia por este generador (no hay lowering de
//               memoria en IrBuilder todavia, ver ir_builder.cpp)
//   X9-X15   <- scratch, libres de usar dentro de un bloque
//   X30/LR   <- direccion de retorno de la llamada BLR que invoco este bloque
//
// Este generador NO reutiliza X20-X27 como scratch (romperia la
// convención para el siguiente bloque) y NO preserva X9-X15 entre
// llamadas (son scratch de verdad, se pisan sin avisar).

namespace pas::cpu::jit {

class Arm64CodeGen {
public:
    // Genera código para 'block' escribiendo en 'emitter'. Devuelve false
    // si el bloque contiene una operación que este generador no sabe
    // traducir todavía (nunca genera código parcial silenciosamente: si
    // falla, el emitter puede tener bytes escritos pero el llamador debe
    // descartar el bloque entero, igual que hace IrBuilder con bloques
    // parciales).
    bool Generate(const ir::Block& block, cpu::arm64::Emitter& emitter);

    // Registro ARM64 (0-31) donde vive el registro x86 dado (0-7, orden
    // ModRM) -- expuesto para que el llamador (tests, futuro dispatcher)
    // pueda leer/escribir el estado inicial antes de invocar el bloque.
    static uint8_t MappedRegister(uint8_t x86_reg_index) { return 20 + x86_reg_index; }

private:
    // Ubicación en tiempo de generación de cada ir::Value ya calculado:
    // el número de registro ARM64 (scratch o pinned) que lo contiene.
    // No hay spill a memoria -- si se agotan los scratch disponibles, se
    // trata como fallo de generación (no debería ocurrir con los bloques
    // pequeños actuales; ver Generate() para el conteo).
    std::unordered_map<uint32_t, uint8_t> value_location_;
    uint8_t next_scratch_ = 9; // X9.. -- ver comentario de cabecera

    uint8_t AllocScratch();
    uint8_t LocationOf(ir::Value v) const;
};

} // namespace pas::cpu::jit
