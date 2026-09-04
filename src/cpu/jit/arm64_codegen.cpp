#include "cpu/jit/arm64_codegen.h"
#include "core/log.h"

namespace pas::cpu::jit {

uint8_t Arm64CodeGen::AllocScratch() {
    // Ciclo simple por X9-X15 (7 registros) -- ver docs/JIT.md, alcance
    // actual: bloques pequeños, nunca deberian necesitar mas de 7 valores
    // vivos simultaneos con el lowering actual de IrBuilder (que no reusa
    // valores entre instrucciones, asi que en la practica el numero de
    // valores vivos a la vez es 1-2). Si algun dia esto no basta, hace
    // falta spill real o un asignador de registros de verdad -- no
    // implementado, se detecta como fallo explicito (ver Generate()).
    uint8_t reg = next_scratch_;
    next_scratch_++;
    return reg;
}

uint8_t Arm64CodeGen::LocationOf(ir::Value v) const {
    auto it = value_location_.find(v.id);
    return it != value_location_.end() ? it->second : 0xFF; // 0xFF = no encontrado
}

bool Arm64CodeGen::Generate(const ir::Block& block, cpu::arm64::Emitter& emitter) {
    for (const auto& inst : block.instructions) {
        if (next_scratch_ > 15) {
            PAS_LOG_ERROR("Arm64CodeGen", "Sin registros scratch disponibles (bloque "
                                          "demasiado grande para el generador actual, "
                                          "ver docs/JIT.md)");
            return false;
        }

        switch (inst.op) {
            case ir::OpCode::LoadImm: {
                uint8_t dst = AllocScratch();
                uint32_t imm = static_cast<uint32_t>(inst.immediate);
                emitter.EmitMovz(dst, static_cast<uint16_t>(imm & 0xFFFF), 0, /*is64=*/false);
                if ((imm >> 16) != 0) {
                    emitter.EmitMovk(dst, static_cast<uint16_t>(imm >> 16), 1, /*is64=*/false);
                }
                value_location_[inst.dst.id] = dst;
                break;
            }

            case ir::OpCode::LoadReg: {
                // "Pinned register": leer un registro x86 no cuesta
                // ninguna instruccion, el Value simplemente ES el
                // registro ARM64 mapeado (ver docs/JIT.md, convencion de
                // registros).
                value_location_[inst.dst.id] = MappedRegister(inst.reg_index);
                break;
            }

            case ir::OpCode::StoreReg: {
                uint8_t src_reg = LocationOf(inst.src[0]);
                if (src_reg == 0xFF) {
                    PAS_LOG_ERROR("Arm64CodeGen", "StoreReg: valor origen no generado todavia");
                    return false;
                }
                uint8_t dst_reg = MappedRegister(inst.reg_index);
                if (src_reg != dst_reg) {
                    // MOV Wd,Wn == ORR Wd,WZR,Wn -- ver docs/JIT.md.
                    emitter.EmitOrrReg(dst_reg, /*rn=*/31, src_reg, /*is64=*/false);
                }
                break;
            }

            case ir::OpCode::Add:
            case ir::OpCode::Sub:
            case ir::OpCode::And:
            case ir::OpCode::Or:
            case ir::OpCode::Xor: {
                uint8_t a = LocationOf(inst.src[0]);
                uint8_t b = LocationOf(inst.src[1]);
                if (a == 0xFF || b == 0xFF) {
                    PAS_LOG_ERROR("Arm64CodeGen", "Operacion binaria con operando no generado");
                    return false;
                }
                uint8_t dst = AllocScratch();
                switch (inst.op) {
                    case ir::OpCode::Add: emitter.EmitAdd(dst, a, b, false); break;
                    case ir::OpCode::Sub: emitter.EmitSub(dst, a, b, false); break;
                    case ir::OpCode::And: emitter.EmitAndReg(dst, a, b, false); break;
                    case ir::OpCode::Or:  emitter.EmitOrrReg(dst, a, b, false); break;
                    case ir::OpCode::Xor: emitter.EmitEorReg(dst, a, b, false); break;
                    default: break;
                }
                value_location_[inst.dst.id] = dst;
                break;
            }

            case ir::OpCode::Return: {
                // Cargar next_guest_address en W0 antes de retornar al dispatcher
                uint32_t next_pc = block.next_guest_address;
                emitter.EmitMovz(0, static_cast<uint16_t>(next_pc & 0xFFFF), 0, /*is64=*/false);
                if ((next_pc >> 16) != 0) {
                    emitter.EmitMovk(0, static_cast<uint16_t>(next_pc >> 16), 1, /*is64=*/false);
                }
                emitter.EmitRet();
                break;
            }

            default:
                PAS_LOG_ERROR("Arm64CodeGen", "Opcode IR sin lowering a ARM64 todavia "
                                              "(ver arm64_codegen.cpp)");
                return false;
        }

        if (emitter.HadEncodingError() || emitter.Overflowed()) {
            PAS_LOG_ERROR("Arm64CodeGen", "El emisor reporto un error al generar este bloque");
            return false;
        }
    }
    return true;
}

} // namespace pas::cpu::jit
