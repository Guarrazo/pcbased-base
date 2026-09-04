#include "cpu/translator/ir_builder.h"
#include "core/log.h"

// Alcance actual (ver docs/ROADMAP.md, docs/JIT.md): lowering directo,
// instruccion a instruccion, SIN dataflow entre instrucciones (cada
// operando se relee con un LoadReg nuevo aunque el valor ya estuviera en
// un Value anterior del mismo bloque). Esto es deliberadamente sub-optimo
// pero correcto y simple -- Arm64CodeGen (src/cpu/jit/arm64_codegen.h)
// trata LoadReg/StoreReg como practicamente gratis (registro x86 fijado a
// un registro ARM64 concreto, ver docs/JIT.md "Convencion de registros"),
// asi que el coste real de esta falta de optimizacion es bajo. Optimizar
// esto (eliminar LoadReg redundantes) es trabajo futuro, no correctitud.
//
// Cubre: MOV reg<-reg, MOV reg<-imm32, Grupo1 reg,imm (ADD/SUB/AND/OR/XOR/CMP).
// NO cubre todavia: cualquier instruccion con operando de memoria (LEA,
// PUSH/POP, MOV con ModRM de memoria) -- se registra como error explicito,
// ver docs/ROADMAP.md sobre por que (falta el diseÃ±o de traduccion de
// direcciones guest->host, ver docs/JIT.md).

namespace pas::cpu::translator {

namespace {

uint32_t g_next_value_id = 1; // reiniciado por bloque, ver BuildBlock()

ir::Value NewValue() {
    ir::Value v;
    v.id = g_next_value_id++;
    return v;
}

ir::Value EmitLoadReg(ir::Block& block, x86::Reg reg) {
    ir::Instruction inst;
    inst.op = ir::OpCode::LoadReg;
    inst.dst = NewValue();
    inst.reg_index = static_cast<uint8_t>(reg);
    block.instructions.push_back(inst);
    return inst.dst;
}

void EmitStoreReg(ir::Block& block, x86::Reg reg, ir::Value src) {
    ir::Instruction inst;
    inst.op = ir::OpCode::StoreReg;
    inst.reg_index = static_cast<uint8_t>(reg);
    inst.src[0] = src;
    block.instructions.push_back(inst);
}

ir::Value EmitLoadImm(ir::Block& block, uint64_t imm) {
    ir::Instruction inst;
    inst.op = ir::OpCode::LoadImm;
    inst.dst = NewValue();
    inst.immediate = imm;
    block.instructions.push_back(inst);
    return inst.dst;
}

ir::Value EmitBinOp(ir::Block& block, ir::OpCode op, ir::Value a, ir::Value b) {
    ir::Instruction inst;
    inst.op = op;
    inst.dst = NewValue();
    inst.src[0] = a;
    inst.src[1] = b;
    block.instructions.push_back(inst);
    return inst.dst;
}

void EmitReturn(ir::Block& block) {
    ir::Instruction inst;
    inst.op = ir::OpCode::Return;
    block.instructions.push_back(inst);
}

// Devuelve el Value que representa el operando fuente 'op' -- solo
// soporta Register/Immediate (memoria: ver comentario de cabecera).
bool LoadOperandValue(ir::Block& block, const x86::X86Operand& op, ir::Value& out) {
    if (op.kind == x86::X86Operand::Kind::Register) {
        out = EmitLoadReg(block, op.reg);
        return true;
    }
    if (op.kind == x86::X86Operand::Kind::Immediate) {
        out = EmitLoadImm(block, static_cast<uint64_t>(op.immediate));
        return true;
    }
    return false; // Memory -- no soportado todavia
}

} // namespace

ir::Block IrBuilder::BuildBlock(const x86::X86Instruction* instructions, size_t count) {
    ir::Block block;
    g_next_value_id = 1; // Values son locales a cada bloque
    if (count > 0) {
        block.guest_address = instructions[0].address;
    }

    for (size_t i = 0; i < count; ++i) {
        const x86::X86Instruction& x = instructions[i];

        switch (x.opcode) {
            case x86::Opcode::Mov: {
                if (x.operands[0].kind != x86::X86Operand::Kind::Register) {
                    PAS_LOG_ERROR("IrBuilder", "MOV con destino no-registro no soportado "
                                               "todavia (addr=0x%08x)", x.address);
                    goto unsupported;
                }
                ir::Value src;
                if (!LoadOperandValue(block, x.operands[1], src)) {
                    PAS_LOG_ERROR("IrBuilder", "MOV con origen de memoria no soportado "
                                               "todavia (addr=0x%08x)", x.address);
                    goto unsupported;
                }
                EmitStoreReg(block, x.operands[0].reg, src);
                break;
            }

            case x86::Opcode::Add:
            case x86::Opcode::Sub:
            case x86::Opcode::And:
            case x86::Opcode::Or:
            case x86::Opcode::Xor:
            case x86::Opcode::Cmp: {
                if (x.operands[0].kind != x86::X86Operand::Kind::Register) {
                    PAS_LOG_ERROR("IrBuilder", "Grupo1 con destino de memoria no soportado "
                                               "todavia (addr=0x%08x)", x.address);
                    goto unsupported;
                }
                ir::Value lhs = EmitLoadReg(block, x.operands[0].reg);
                ir::Value rhs;
                if (!LoadOperandValue(block, x.operands[1], rhs)) {
                    PAS_LOG_ERROR("IrBuilder", "Grupo1 con segundo operando de memoria no "
                                               "soportado todavia (addr=0x%08x)", x.address);
                    goto unsupported;
                }

                ir::OpCode ir_op;
                switch (x.opcode) {
                    case x86::Opcode::Add: ir_op = ir::OpCode::Add; break;
                    case x86::Opcode::Sub: ir_op = ir::OpCode::Sub; break;
                    case x86::Opcode::And: ir_op = ir::OpCode::And; break;
                    case x86::Opcode::Or:  ir_op = ir::OpCode::Or;  break;
                    case x86::Opcode::Xor: ir_op = ir::OpCode::Xor; break;
                    case x86::Opcode::Cmp: ir_op = ir::OpCode::Sub; break; // CMP = SUB descartando el resultado
                    default: ir_op = ir::OpCode::Nop; break;
                }
                ir::Value result = EmitBinOp(block, ir_op, lhs, rhs);

                if (x.opcode != x86::Opcode::Cmp) {
                    EmitStoreReg(block, x.operands[0].reg, result);
                }
                // TODO: CompareAndSetFlags -- no emitido todavia porque
                // nada lo consume aun (Jcc no esta decodificado, ver
                // docs/ROADMAP.md). Cuando se aÃ±ada Jcc, esta es la
                // instruccion Add/Sub/And/etc. de arriba la que debe
                // llevar aparejado un CompareAndSetFlags.
                break;
            }

            case x86::Opcode::Ret:
                EmitReturn(block);
                break;

            default:
                PAS_LOG_ERROR("IrBuilder", "Opcode x86 sin lowering a IR todavia (addr=0x%08x) "
                                           "-- ver ir_builder.cpp para el alcance actual",
                              x.address);
                goto unsupported;
        }
    }

    // Calcular next_guest_address para el dispatcher
    if (count > 0) {
        const auto& last_inst = instructions[count - 1];
        if (last_inst.opcode == x86::Opcode::Ret) {
            block.next_guest_address = 0xFFFFFFFF;  // sentinel: terminar
        } else {
            // Dirección después de la última instrucción del bloque
            block.next_guest_address = last_inst.address + last_inst.length;
        }
    }

    return block;

unsupported:
    // Se devuelve el bloque parcial construido hasta el punto de fallo --
    // el llamador (jit.cpp) debe comprobar esto y tratarlo como fallo de
    // traduccion del bloque completo, no ejecutar un bloque a medias.
    return block;
}

void IrBuilder::ApplyPatches(ir::Block& /*block*/) {
    // TODO: consultar patch::PatchEngine (src/patch/) por patches que
    // afecten a block.guest_address. Ver docs/PATCHING.md.
}

} // namespace pas::cpu::translator
