#include "cpu/jit/arm64_codegen.h"
#include "core/log.h"

namespace pas::cpu::jit {

static constexpr int kOffsetRegs = 0;
static constexpr int kOffsetEflags = 32;
static constexpr int kOffsetPc = 36;
static constexpr int kOffsetMemBase = 40;

static constexpr uint8_t kEax = 19, kEcx = 20, kEdx = 21, kEbx = 22,
                           kEsp = 23, kEbp = 24, kEsi = 25, kEdi = 26,
                           kEflags = 27, kRebase = 28, kState = 0;

Arm64CodeGen::Arm64CodeGen(uint8_t* mem_base, uint32_t mem_base_guest)
    : mem_base_(mem_base), mem_base_guest_(mem_base_guest) {}

void Arm64CodeGen::EmitPrologue(cpu::arm64::Emitter& e) {
    for (int i = 0; i < 8; ++i) {
        e.EmitLdrImm(kEax + i, kState, kOffsetRegs + i * 4, 4);
    }
    e.EmitLdrImm(kEflags, kState, kOffsetEflags, 4);
    e.EmitLdrImm(kRebase, kState, kOffsetMemBase, 8);
}

void Arm64CodeGen::EmitEpilogue(cpu::arm64::Emitter& e, uint32_t next_pc) {
    for (int i = 0; i < 8; ++i) {
        e.EmitStrImm(kEax + i, kState, kOffsetRegs + i * 4, 4);
    }
    e.EmitStrImm(kEflags, kState, kOffsetEflags, 4);
    uint8_t scratch = AllocScratch();
    if (scratch != 0xFF) {
        e.EmitMovz(scratch, static_cast<uint16_t>(next_pc & 0xFFFF), 0, false);
        if ((next_pc >> 16) != 0) {
            e.EmitMovk(scratch, static_cast<uint16_t>(next_pc >> 16), 1, false);
        }
        e.EmitStrImm(scratch, kState, kOffsetPc, 4);
    }
    e.EmitRet();
}

uint8_t Arm64CodeGen::AllocScratch() {
    if (next_scratch_ > 15) return 0xFF;
    return next_scratch_++;
}

uint8_t Arm64CodeGen::LocationOf(ir::Value v) const {
    auto it = value_location_.find(v.id);
    return it != value_location_.end() ? it->second : 0xFF;
}

bool Arm64CodeGen::EmitEffectiveAddress(const ir::Instruction& inst, uint8_t out_reg,
                                        cpu::arm64::Emitter& e) {
    if (inst.mem_has_base) {
        uint8_t base = MappedRegister(inst.mem_base_reg);
        if (base != out_reg) {
            e.EmitOrrReg(out_reg, 31, base, false);
        }
    } else {
        e.EmitMovz(out_reg, 0, 0, false);
    }

    if (inst.mem_has_index) {
        uint8_t idx = MappedRegister(inst.mem_index_reg);
        if (inst.mem_scale == 1) {
            e.EmitAdd(out_reg, out_reg, idx, false);
        } else {
            uint8_t shifted = AllocScratch();
            if (shifted == 0xFF) return false;
            uint8_t log2 = (inst.mem_scale == 2) ? 1 : (inst.mem_scale == 4) ? 2 : 3;
            e.EmitLsl(shifted, idx, log2, false);
            e.EmitAdd(out_reg, out_reg, shifted, false);
        }
    }

    int32_t disp = inst.mem_disp;
    if (disp != 0) {
        if (disp >= 0 && static_cast<uint32_t>(disp) <= 4095) {
            e.EmitAddImm(out_reg, out_reg, static_cast<uint32_t>(disp), false);
        } else if (disp < 0 && static_cast<int64_t>(-disp) <= 4095) {
            e.EmitSubImm(out_reg, out_reg, static_cast<uint32_t>(-disp), false);
        } else {
            uint8_t imm_reg = AllocScratch();
            if (imm_reg == 0xFF) return false;
            uint32_t udisp = static_cast<uint32_t>(disp);
            e.EmitMovz(imm_reg, static_cast<uint16_t>(udisp & 0xFFFF), 0, false);
            if ((udisp >> 16) != 0) {
                e.EmitMovk(imm_reg, static_cast<uint16_t>(udisp >> 16), 1, false);
            }
            e.EmitAdd(out_reg, out_reg, imm_reg, false);
        }
    }
    return true;
}

bool Arm64CodeGen::Generate(const ir::Block& block, cpu::arm64::Emitter& e) {
    value_location_.clear();
    next_scratch_ = 9;

    EmitPrologue(e);

    for (const auto& inst : block.instructions) {
        if (next_scratch_ > 15) {
            PAS_LOG_ERROR("Arm64CodeGen", "Sin scratch disponibles");
            return false;
        }

        switch (inst.op) {
            case ir::OpCode::LoadImm: {
                uint8_t dst = AllocScratch();
                uint32_t imm = static_cast<uint32_t>(inst.immediate);
                e.EmitMovz(dst, static_cast<uint16_t>(imm & 0xFFFF), 0, false);
                if ((imm >> 16) != 0) {
                    e.EmitMovk(dst, static_cast<uint16_t>(imm >> 16), 1, false);
                }
                value_location_[inst.dst.id] = dst;
                break;
            }

            case ir::OpCode::LoadReg: {
                value_location_[inst.dst.id] = MappedRegister(inst.reg_index);
                break;
            }

            case ir::OpCode::StoreReg: {
                uint8_t src = LocationOf(inst.src[0]);
                if (src == 0xFF) {
                    PAS_LOG_ERROR("Arm64CodeGen", "StoreReg: valor origen no generado");
                    return false;
                }
                uint8_t dst = MappedRegister(inst.reg_index);
                if (src != dst) {
                    e.EmitOrrReg(dst, 31, src, false);
                }
                break;
            }

            case ir::OpCode::LoadMem: {
                uint8_t addr = AllocScratch();
                if (addr == 0xFF) return false;
                if (!EmitEffectiveAddress(inst, addr, e)) return false;
                uint8_t host = AllocScratch();
                if (host == 0xFF) return false;
                e.EmitAdd(host, kRebase, addr, true);
                uint8_t dst = AllocScratch();
                e.EmitLdrImm(dst, host, 0, inst.mem_size);
                value_location_[inst.dst.id] = dst;
                break;
            }

            case ir::OpCode::StoreMem: {
                uint8_t src = LocationOf(inst.src[0]);
                if (src == 0xFF) return false;
                uint8_t addr = AllocScratch();
                if (addr == 0xFF) return false;
                if (!EmitEffectiveAddress(inst, addr, e)) return false;
                uint8_t host = AllocScratch();
                if (host == 0xFF) return false;
                e.EmitAdd(host, kRebase, addr, true);
                e.EmitStrImm(src, host, 0, inst.mem_size);
                break;
            }

            case ir::OpCode::Lea: {
                uint8_t dst = AllocScratch();
                if (dst == 0xFF) return false;
                if (!EmitEffectiveAddress(inst, dst, e)) return false;
                value_location_[inst.dst.id] = dst;
                break;
            }

            case ir::OpCode::Push: {
                uint8_t src = LocationOf(inst.src[0]);
                if (src == 0xFF) return false;
                e.EmitSubImm(kEsp, kEsp, 4, false);
                uint8_t host = AllocScratch();
                if (host == 0xFF) return false;
                e.EmitAdd(host, kRebase, kEsp, true);
                e.EmitStrImm(src, host, 0, 4);
                break;
            }

            case ir::OpCode::Pop: {
                uint8_t host = AllocScratch();
                if (host == 0xFF) return false;
                e.EmitAdd(host, kRebase, kEsp, true);
                uint8_t dst = AllocScratch();
                e.EmitLdrImm(dst, host, 0, 4);
                value_location_[inst.dst.id] = dst;
                e.EmitAddImm(kEsp, kEsp, 4, false);
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
                    case ir::OpCode::Add: e.EmitAdd(dst, a, b, false); break;
                    case ir::OpCode::Sub: e.EmitSub(dst, a, b, false); break;
                    case ir::OpCode::And: e.EmitAndReg(dst, a, b, false); break;
                    case ir::OpCode::Or:  e.EmitOrrReg(dst, a, b, false); break;
                    case ir::OpCode::Xor: e.EmitEorReg(dst, a, b, false); break;
                    default: break;
                }
                value_location_[inst.dst.id] = dst;
                break;
            }

            case ir::OpCode::AddImm: {
                uint8_t a = LocationOf(inst.src[0]);
                if (a == 0xFF) return false;
                uint32_t imm = static_cast<uint32_t>(inst.immediate);
                uint8_t dst = AllocScratch();
                if (imm <= 4095) {
                    e.EmitAddImm(dst, a, imm, false);
                } else {
                    uint8_t imm_reg = AllocScratch();
                    e.EmitMovz(imm_reg, static_cast<uint16_t>(imm & 0xFFFF), 0, false);
                    if ((imm >> 16) != 0) {
                        e.EmitMovk(imm_reg, static_cast<uint16_t>(imm >> 16), 1, false);
                    }
                    e.EmitAdd(dst, a, imm_reg, false);
                }
                value_location_[inst.dst.id] = dst;
                break;
            }

            case ir::OpCode::Return: {
                break;
            }

            default:
                PAS_LOG_ERROR("Arm64CodeGen", "Opcode IR sin lowering: %d",
                              static_cast<int>(inst.op));
                return false;
        }

        if (e.HadEncodingError() || e.Overflowed()) {
            PAS_LOG_ERROR("Arm64CodeGen", "El emisor reporto error");
            return false;
        }
    }

    EmitEpilogue(e, block.next_guest_address);
    return !(e.HadEncodingError() || e.Overflowed());
}

} // namespace pas::cpu::jit
