#include "cpu/translator/ir_builder.h"
#include "core/log.h"

namespace pas::cpu::translator {

namespace {

class BlockBuilder {
public:
    ir::Block block;
    uint32_t next_value_id_ = 1;

    ir::Value NextValue() {
        ir::Value v;
        v.id = next_value_id_++;
        return v;
    }

    ir::Value MakeRegValue(x86::Reg r) {
        ir::Instruction load;
        load.op = ir::OpCode::LoadReg;
        load.reg_index = static_cast<uint8_t>(r);
        load.dst = NextValue();
        block.instructions.push_back(load);
        return load.dst;
    }

    ir::Value MakeRegOrImm(const x86::X86Operand& op) {
        if (op.kind == x86::X86Operand::Kind::Immediate) {
            ir::Instruction load;
            load.op = ir::OpCode::LoadImm;
            load.immediate = op.immediate;
            load.dst = NextValue();
            block.instructions.push_back(load);
            return load.dst;
        }
        return MakeRegValue(op.reg);
    }

    void FillMemOperand(ir::Instruction& ir, const x86::X86Operand& mem) {
        ir.mem_size = static_cast<uint8_t>(mem.size);
        if (mem.has_base) {
            ir.mem_has_base = true;
            ir.mem_base_reg = static_cast<uint8_t>(mem.base_reg);
        }
        if (mem.has_index) {
            ir.mem_has_index = true;
            ir.mem_index_reg = static_cast<uint8_t>(mem.index_reg);
            ir.mem_scale = mem.scale;
        }
        ir.mem_disp = mem.disp;
    }

    void AppendStoreReg(x86::Reg r, ir::Value v) {
        ir::Instruction store;
        store.op = ir::OpCode::StoreReg;
        store.reg_index = static_cast<uint8_t>(r);
        store.src[0] = v;
        block.instructions.push_back(store);
    }
};

} // namespace

ir::Block IrBuilder::BuildBlock(const x86::X86Instruction* instructions, size_t count) {
    BlockBuilder b;
    b.block.guest_address = instructions[0].address;

    for (size_t i = 0; i < count; ++i) {
        const auto& inst = instructions[i];

        switch (inst.opcode) {
            case x86::Opcode::Mov: {
                if (inst.operands[0].kind == x86::X86Operand::Kind::Memory &&
                    inst.operands[1].kind == x86::X86Operand::Kind::Register) {
                    ir::Instruction ir;
                    ir.op = ir::OpCode::StoreMem;
                    ir.src[0] = b.MakeRegValue(inst.operands[1].reg);
                    b.FillMemOperand(ir, inst.operands[0]);
                    b.block.instructions.push_back(ir);
                } else if (inst.operands[0].kind == x86::X86Operand::Kind::Register &&
                           inst.operands[1].kind == x86::X86Operand::Kind::Memory) {
                    ir::Instruction ir;
                    ir.op = ir::OpCode::LoadMem;
                    b.FillMemOperand(ir, inst.operands[1]);
                    ir.dst = b.NextValue();
                    b.block.instructions.push_back(ir);
                    b.AppendStoreReg(inst.operands[0].reg, ir.dst);
                } else if (inst.operands[0].kind == x86::X86Operand::Kind::Register &&
                           inst.operands[1].kind == x86::X86Operand::Kind::Immediate) {
                    ir::Instruction ir;
                    ir.op = ir::OpCode::LoadImm;
                    ir.immediate = inst.operands[1].immediate;
                    ir.dst = b.NextValue();
                    b.block.instructions.push_back(ir);
                    b.AppendStoreReg(inst.operands[0].reg, ir.dst);
                } else if (inst.operands[0].kind == x86::X86Operand::Kind::Register &&
                           inst.operands[1].kind == x86::X86Operand::Kind::Register) {
                    ir::Value src = b.MakeRegValue(inst.operands[1].reg);
                    b.AppendStoreReg(inst.operands[0].reg, src);
                } else {
                    PAS_LOG_WARN("IrBuilder", "MOV con combinacion no soportada");
                }
                break;
            }

            case x86::Opcode::Lea: {
                if (inst.operands[1].kind != x86::X86Operand::Kind::Memory) {
                    PAS_LOG_ERROR("IrBuilder", "LEA con operando no-memoria");
                    break;
                }
                ir::Instruction ir;
                ir.op = ir::OpCode::Lea;
                b.FillMemOperand(ir, inst.operands[1]);
                ir.dst = b.NextValue();
                b.block.instructions.push_back(ir);
                b.AppendStoreReg(inst.operands[0].reg, ir.dst);
                break;
            }

            case x86::Opcode::Push: {
                ir::Instruction ir;
                ir.op = ir::OpCode::Push;
                ir.src[0] = b.MakeRegOrImm(inst.operands[0]);
                b.block.instructions.push_back(ir);
                break;
            }

            case x86::Opcode::Pop: {
                ir::Instruction ir;
                ir.op = ir::OpCode::Pop;
                ir.dst = b.NextValue();
                b.block.instructions.push_back(ir);
                b.AppendStoreReg(inst.operands[0].reg, ir.dst);
                break;
            }

            case x86::Opcode::Add:
            case x86::Opcode::Sub:
            case x86::Opcode::And:
            case x86::Opcode::Or:
            case x86::Opcode::Xor:
            case x86::Opcode::Cmp: {
                ir::OpCode ir_op;
                switch (inst.opcode) {
                    case x86::Opcode::Add: ir_op = ir::OpCode::Add; break;
                    case x86::Opcode::Sub: ir_op = ir::OpCode::Sub; break;
                    case x86::Opcode::And: ir_op = ir::OpCode::And; break;
                    case x86::Opcode::Or:  ir_op = ir::OpCode::Or; break;
                    case x86::Opcode::Xor: ir_op = ir::OpCode::Xor; break;
                    case x86::Opcode::Cmp: ir_op = ir::OpCode::Sub; break;
                    default: ir_op = ir::OpCode::Nop; break;
                }

                ir::Value lhs = b.MakeRegOrImm(inst.operands[0]);
                ir::Value rhs = b.MakeRegOrImm(inst.operands[1]);
                ir::Instruction ir;
                ir.op = ir_op;
                ir.src[0] = lhs;
                ir.src[1] = rhs;
                ir.dst = b.NextValue();
                b.block.instructions.push_back(ir);

                if (inst.opcode != x86::Opcode::Cmp) {
                    if (inst.operands[0].kind == x86::X86Operand::Kind::Register) {
                        b.AppendStoreReg(inst.operands[0].reg, ir.dst);
                    } else if (inst.operands[0].kind == x86::X86Operand::Kind::Memory) {
                        ir::Instruction store;
                        store.op = ir::OpCode::StoreMem;
                        store.src[0] = ir.dst;
                        b.FillMemOperand(store, inst.operands[0]);
                        b.block.instructions.push_back(store);
                    }
                }
                break;
            }

            case x86::Opcode::Call: {
                ir::Instruction ir;
                ir.op = ir::OpCode::Call;
                ir.immediate = inst.operands[0].immediate;
                b.block.instructions.push_back(ir);
                break;
            }

            case x86::Opcode::Ret: {
                ir::Instruction ir;
                ir.op = ir::OpCode::Return;
                b.block.instructions.push_back(ir);
                break;
            }

            case x86::Opcode::Nop: {
                ir::Instruction ir;
                ir.op = ir::OpCode::Nop;
                b.block.instructions.push_back(ir);
                break;
            }

            default:
                PAS_LOG_WARN("IrBuilder", "Opcode x86 no mapeado: %d",
                             static_cast<int>(inst.opcode));
                break;
        }
    }

    if (count > 0) {
        const auto& last = instructions[count - 1];
        b.block.next_guest_address = last.address + last.length;
    }

    return b.block;
}

void IrBuilder::ApplyPatches(ir::Block& block) {
    (void)block;
}

} // namespace pas::cpu::translator
