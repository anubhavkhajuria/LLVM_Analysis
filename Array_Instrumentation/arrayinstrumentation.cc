#include <map>
#include <queue>
#include <set>
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace {


class range {
private:
    int safelane, offlane; 

public:
    range(int pos) : safelane(pos), offlane(pos) {};
    range(int safe, int off) : safelane(safe), offlane(off) {};
    range();

    static range add(range r1, range r2);
    static range sub(range r1, range r2);
    static range mul(range r1, range r2);
    static range join(range r1, range r2);
    static range intersect(range r1, range r2);

    void print(raw_ostream &os);
    int get_low() const { return safelane; }
    int get_high() const { return offlane; }
    
    friend bool operator< (const range& r1, const range& r2);
    friend bool operator==(const range& r1, const range& r2);
    friend bool operator!=(const range& r1, const range& r2);
};

static range r_top(INT_MIN, INT_MAX);
static range r_bot(INT_MAX, INT_MIN);

range::range() {
    safelane = r_bot.safelane;
    offlane = r_bot.offlane;
}

int64_t clamp_i32(int64_t val) {
    if (val > INT_MAX) return INT_MAX;
    if (val < INT_MIN) return INT_MIN;
    return val;
}

void range::print(raw_ostream &os) { 
    if (*this == r_bot) os << "[BOT]";
    else os << "[" << safelane << "," << offlane << "]";
}

range range::add(range r1, range r2) {
    if (r1 == r_bot || r2 == r_bot) return r_bot;
    int64_t new_safe = clamp_i32((int64_t)r1.safelane + r2.safelane);
    int64_t new_off = clamp_i32((int64_t)r1.offlane + r2.offlane);
    return range(new_safe, new_off);
}

range range::sub(range r1, range r2) {
    if (r1 == r_bot || r2 == r_bot) return r_bot;
    int64_t new_safe = clamp_i32((int64_t)r1.safelane - r2.offlane);
    int64_t new_off = clamp_i32((int64_t)r1.offlane - r2.safelane);
    return range(new_safe, new_off);
}

range range::mul(range r1, range r2) {
    if (r1 == r_bot || r2 == r_bot) return r_bot;
    int64_t t1 = clamp_i32((int64_t)r1.safelane * r2.safelane);
    int64_t t2 = clamp_i32((int64_t)r1.safelane * r2.offlane);
    int64_t t3 = clamp_i32((int64_t)r1.offlane * r2.safelane);
    int64_t t4 = clamp_i32((int64_t)r1.offlane * r2.offlane);

    int64_t min_val = std::min({t1, t2, t3, t4});
    int64_t max_val = std::max({t1, t2, t3, t4});
    return range(min_val, max_val);
}

range range::join(range r1, range r2) {
    if (r1 == r_bot) return r2;
    if (r2 == r_bot) return r1;
    int64_t new_safe = std::min(r1.safelane, r2.safelane);
    int64_t new_off = std::max(r1.offlane, r2.offlane);
    return range(new_safe, new_off);
}

range range::intersect(range r1, range r2) {
    if (r1 == r_bot || r2 == r_bot) return r_bot;
    int64_t new_safe = std::max(r1.safelane, r2.safelane);
    int64_t new_off = std::min(r1.offlane, r2.offlane);
    if (new_safe > new_off) return r_bot;
    return range(new_safe, new_off);
}

raw_ostream& operator<<(raw_ostream& os, range r) { r.print(os); return os; }

bool operator< (const range& r1, const range& r2) {
    if (r1 == r_bot) return true;
    if (r2 == r_bot) return false;
    return std::tie(r1.safelane, r1.offlane) < std::tie(r2.safelane, r2.offlane);
}

bool operator==(const range& r1, const range& r2) {
    return std::tie(r1.safelane, r1.offlane) == std::tie(r2.safelane, r2.offlane);
}

bool operator!=(const range& r1, const range& r2) {
    return std::tie(r1.safelane, r1.offlane) != std::tie(r2.safelane, r2.offlane);
}


class block_state;

//forward declaration
range get_range(Value *val, block_state &state);

class arr_state {
public:
    std::map<int64_t, range> elem_map;
    range default_r;
    
    arr_state() : default_r(range(0, 0)) {}
    
    void store_elem(Value* idx, range val, block_state& state);
    range load_elem(Value* idx, block_state& state);
    static arr_state join_arr(const arr_state& s1, const arr_state& s2);
};


class block_state {
public:
    std::map<Value*, range> val_ranges;
    std::map<Value*, arr_state> arr_states;
    bool reachable;

    block_state() : reachable(false) {}

    bool join_state(const block_state &other);
    bool operator!=(const block_state &other) const;
};

bool block_state::join_state(const block_state &other) {
    if (!other.reachable) {
        return false;
    }
    
    if (!reachable) {
        reachable = true;
        val_ranges = other.val_ranges;
        arr_states = other.arr_states;
        return true;
    }
    
    bool changed = false;
    
    for (const auto& [val, other_r] : other.val_ranges) {
        if (other_r == r_bot) continue;
        
        auto it = val_ranges.find(val);
        range old_r = (it != val_ranges.end()) ? it->second : r_bot;
        range new_r = range::join(old_r, other_r);
        
        if (it == val_ranges.end() || old_r != new_r) {
            val_ranges[val] = new_r;
            changed = true;
        }
    }
    
    for (const auto& [alloc, other_arr] : other.arr_states) {
        auto it = arr_states.find(alloc);
        arr_state new_arr = (it != arr_states.end())
            ? arr_state::join_arr(it->second, other_arr)
            : other_arr;
        
        bool needs_update = (it == arr_states.end()) ||
                           (it->second.default_r != new_arr.default_r) ||
                           (it->second.elem_map != new_arr.elem_map);
        
        if (needs_update) {
            arr_states[alloc] = new_arr;
            changed = true;
        }
    }
        
    return changed;
}


bool block_state::operator!=(const block_state &other) const {
    if (reachable != other.reachable) return true;
    if (!reachable && !other.reachable) return false;
    
    if (val_ranges.size() != other.val_ranges.size()) return true;
    if (arr_states.size() != other.arr_states.size()) return true;
    
    for (auto const& [val, r] : val_ranges) {
        auto it = other.val_ranges.find(val);
        if (it == other.val_ranges.end() || it->second != r) {
            return true;
        }
    }
    
    for (auto const& [alloc, arr] : arr_states) {
        auto it = other.arr_states.find(alloc);
        if (it == other.arr_states.end() || 
            it->second.default_r != arr.default_r ||
            it->second.elem_map.size() != arr.elem_map.size()) {
            return true;
        }
    }
    
    return false;
}

void arr_state::store_elem(Value* idx, range val, block_state& state) {
    if (ConstantInt *cint = dyn_cast<ConstantInt>(idx)) {
        int64_t i = cint->getSExtValue();
        elem_map[i] = val;
    } else if (ConstantExpr *ce = dyn_cast<ConstantExpr>(idx)) {
        default_r = range::join(default_r, val);
        elem_map.clear();
    } else if (BinaryOperator *binop = dyn_cast<BinaryOperator>(idx)) {
        range idx_r = get_range(idx, state);
        if (idx_r.get_low() == idx_r.get_high() && idx_r != r_bot) {
            elem_map[idx_r.get_low()] = val;
        } else {
            default_r = range::join(default_r, val);
            elem_map.clear();
        }
    } else if (state.val_ranges.count(idx)) {
        range idx_r = state.val_ranges[idx];
        if (idx_r.get_low() == idx_r.get_high() && idx_r != r_bot) {
            elem_map[idx_r.get_low()] = val;
        } else {
            default_r = range::join(default_r, val);
            elem_map.clear();
        }
    } else {
        default_r = range::join(default_r, val);
        elem_map.clear();
    }
}

range arr_state::load_elem(Value* idx, block_state& state) {
    if (ConstantInt *cint = dyn_cast<ConstantInt>(idx)) {
        int64_t i = cint->getSExtValue();
        if (elem_map.count(i)) return elem_map[i];
        return default_r;
    } else if (ConstantExpr *ce = dyn_cast<ConstantExpr>(idx)) {
        return default_r;
    } else if (BinaryOperator *binop = dyn_cast<BinaryOperator>(idx)) {
        range idx_r = get_range(idx, state);
        if (idx_r.get_low() == idx_r.get_high() && idx_r != r_bot) {
            int64_t i = idx_r.get_low();
            if (elem_map.count(i)) return elem_map[i];
        }
        return default_r;
    } else if (state.val_ranges.count(idx)) {
        range idx_r = state.val_ranges[idx];
        if (idx_r.get_low() == idx_r.get_high() && idx_r != r_bot) {
            int64_t i = idx_r.get_low();
            if (elem_map.count(i)) return elem_map[i];
        }
    }
    return default_r;
}

arr_state arr_state::join_arr(const arr_state& s1, const arr_state& s2) {
    arr_state result;
    result.default_r = range::join(s1.default_r, s2.default_r);
    
    for (auto const& [idx, r1] : s1.elem_map) {
        if (s2.elem_map.count(idx)) {
            result.elem_map[idx] = range::join(r1, s2.elem_map.at(idx));
        } else {
            result.elem_map[idx] = range::join(r1, s2.default_r);
        }
    }
    
    for (auto const& [idx, r2] : s2.elem_map) {
        if (!s1.elem_map.count(idx)) {
            result.elem_map[idx] = range::join(s1.default_r, r2);
        }
    }
    
    return result;
}

Value* get_base_ptr(Value *ptr) {
    if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(ptr)) {
        return gep->getPointerOperand();
    }
    return ptr;
}

range get_range(Value *val, block_state &state) {
    if (!state.reachable) return r_bot;
    
    if (ConstantInt *cint = dyn_cast<ConstantInt>(val)) {
        return range(cint->getSExtValue(), cint->getSExtValue());
    }
    
    if (ConstantExpr *ce = dyn_cast<ConstantExpr>(val)) {
        return r_top;
    }
    
    if (BinaryOperator *binop = dyn_cast<BinaryOperator>(val)) {
        if (state.val_ranges.count(binop)) {
            return state.val_ranges[binop];
        }
        range r1 = get_range(binop->getOperand(0), state);
        range r2 = get_range(binop->getOperand(1), state);
        switch (binop->getOpcode()) {
            case Instruction::Add: 
                return range::add(r1, r2);
            case Instruction::Sub: 
                return range::sub(r1, r2);
            case Instruction::Mul: 
                return range::mul(r1, r2);
            default: 
                return r_top;
        }
    }
    
    if (LoadInst* load = dyn_cast<LoadInst>(val)) {
        Value* ptr = load->getPointerOperand();
        Value* base = get_base_ptr(ptr);
        
        if (ptr != base && state.arr_states.count(base)) {
            if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(ptr)) {
                if (gep->getNumIndices() >= 2) {
                    auto idx_it = gep->idx_begin();
                    ++idx_it;
                    Value *idx = idx_it->get();
                    return state.arr_states[base].load_elem(idx, state);
                }
            }
        }
        
        if (state.val_ranges.count(base)) return state.val_ranges[base];
    }
    
    if (state.val_ranges.count(val)) return state.val_ranges[val];
    if (Argument *arg = dyn_cast<Argument>(val)) {
        if (state.val_ranges.count(arg)) return state.val_ranges[arg];
    }
    
    return r_top;
}

range get_pred_range(Value *val, BasicBlock* pred_bb, std::map<BasicBlock*, block_state> &out_states) {
    if (!out_states[pred_bb].reachable) return r_bot;
    
    if (ConstantInt *cint = dyn_cast<ConstantInt>(val)) {
        return range(cint->getSExtValue(), cint->getSExtValue());
    }
    
    if (ConstantExpr *ce = dyn_cast<ConstantExpr>(val)) {
        return r_top;
    }
    
    if (BinaryOperator *binop = dyn_cast<BinaryOperator>(val)) {
        if (out_states[pred_bb].val_ranges.count(binop)) {
            return out_states[pred_bb].val_ranges[binop];
        }
        range r1 = get_pred_range(binop->getOperand(0), pred_bb, out_states);
        range r2 = get_pred_range(binop->getOperand(1), pred_bb, out_states);
        switch (binop->getOpcode()) {
            case Instruction::Add: 
                return range::add(r1, r2);
            case Instruction::Sub: 
                return range::sub(r1, r2);
            case Instruction::Mul: 
                return range::mul(r1, r2);
            default: 
                return r_top;
        }
    }
    
    if (out_states[pred_bb].val_ranges.count(val)) {
        return out_states[pred_bb].val_ranges[val];
    }
    
    if(LoadInst* load = dyn_cast<LoadInst>(val)){
        Value* ptr = load->getPointerOperand();
        Value* base = get_base_ptr(ptr);
        
        if (ptr != base && out_states[pred_bb].arr_states.count(base)) {
            if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(ptr)) {
                if (gep->getNumIndices() >= 2) {
                    auto idx_it = gep->idx_begin();
                    ++idx_it;
                    Value *idx = idx_it->get();
                    return out_states[pred_bb].arr_states[base].load_elem(idx, out_states[pred_bb]);
                }
            }
        }
        
        if (out_states[pred_bb].val_ranges.count(base)) return out_states[pred_bb].val_ranges[base];
    }
    
    return r_top;
}

void transfer_inst(Instruction &inst, block_state &current_state, 
                                std::map<BasicBlock*, block_state> &out_states) {
    if (PHINode *phi = dyn_cast<PHINode>(&inst)) {
        range merged = r_bot;
        for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            Value* val = phi->getIncomingValue(i);
            BasicBlock* pred_bb = phi->getIncomingBlock(i);
            if (out_states[pred_bb].reachable) {
                range pred_r = get_pred_range(val, pred_bb, out_states);
                merged = range::join(merged, pred_r);
            }
        }
        current_state.val_ranges[phi] = merged;
    } 
    else if (AllocaInst *alloc = dyn_cast<AllocaInst>(&inst)) {
        if (alloc->getAllocatedType()->isArrayTy()) {
            arr_state new_arr;
            new_arr.default_r = range(0, 0);
            current_state.arr_states[alloc] = new_arr;
        } else {
            current_state.val_ranges[alloc] = r_top;
        }
    } 
    else if (LoadInst *load = dyn_cast<LoadInst>(&inst)) {
        Value *ptr = load->getPointerOperand();
        Value *base = get_base_ptr(ptr);
        
        if (ptr != base && current_state.arr_states.count(base)) {
            if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(ptr)) {
                if (gep->getNumIndices() >= 2) {
                    auto idx_it = gep->idx_begin();
                    ++idx_it;
                    Value *idx = idx_it->get();
                    current_state.val_ranges[load] = current_state.arr_states[base].load_elem(idx, current_state);
                }
            }
        } else {
            current_state.val_ranges[load] = get_range(base, current_state);
        }
    }
    else if (StoreInst *store = dyn_cast<StoreInst>(&inst)) {
        Value *val_op = store->getValueOperand();
        Value *ptr_op = store->getPointerOperand();
        Value *base = get_base_ptr(ptr_op);
        
        range val_r = get_range(val_op, current_state);
        
        if (ptr_op != base) {
            if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(ptr_op)) {
                if (current_state.arr_states.count(base) && gep->getNumIndices() >= 2) {
                    auto idx_it = gep->idx_begin();
                    ++idx_it;
                    Value *idx = idx_it->get();
                    current_state.arr_states[base].store_elem(idx, val_r, current_state);
                } else {
                    current_state.val_ranges[base] = val_r;
                }
            } else {
                current_state.val_ranges[base] = val_r;
            }
        } else {
            current_state.val_ranges[base] = val_r;
        }
    } 
    else if (CallInst *call = dyn_cast<CallInst>(&inst)) {
        Function *fun = call->getCalledFunction();
        if (fun) {
            for (unsigned i = 0; i < call->arg_size(); ++i) {
                Value *arg = call->getArgOperand(i);
                if (arg->getType()->isPointerTy()) {
                    Value *base = get_base_ptr(arg);
                    if (current_state.arr_states.count(base)) {
                        current_state.arr_states[base].default_r = r_top;
                        current_state.arr_states[base].elem_map.clear();
                    } else {
                        current_state.val_ranges[base] = r_top;
                    }
                }
            }
        }
        current_state.val_ranges[call] = r_top;
    } 
    else if (BinaryOperator *binop = dyn_cast<BinaryOperator>(&inst)) {
        range r1 = get_range(binop->getOperand(0), current_state);
        range r2 = get_range(binop->getOperand(1), current_state);
        switch (binop->getOpcode()) {
            case Instruction::Add: 
                current_state.val_ranges[binop] = range::add(r1, r2);
                break;
            case Instruction::Sub: 
                current_state.val_ranges[binop] = range::sub(r1, r2);
                break;
            case Instruction::Mul: 
                current_state.val_ranges[binop] = range::mul(r1, r2);
                break;
            default: 
                current_state.val_ranges[binop] = r_top;
        }
    } 
    else if (SelectInst *sel = dyn_cast<SelectInst>(&inst)) {
        Value *v1 = sel->getTrueValue();
        Value *v2 = sel->getFalseValue();
        range r1 = get_range(v1, current_state);
        range r2 = get_range(v2, current_state);
        current_state.val_ranges[sel] = range::join(r1, r2);
    } 
    else if (CastInst *cast = dyn_cast<CastInst>(&inst)) {
        Value *op = cast->getOperand(0);
        current_state.val_ranges[cast] = get_range(op, current_state);
    }
}

block_state refine_br(BranchInst *br, BasicBlock *succ, 
                                 block_state pred_state) {
    if (!br->isConditional()) return pred_state;
    
    ICmpInst *cmp = dyn_cast<ICmpInst>(br->getCondition());
    if (!cmp) return pred_state;
    
    Value* lhs = cmp->getOperand(0);
    Value* rhs = cmp->getOperand(1);
    
    Value* var = nullptr;
    ConstantInt* cint = nullptr;
    ICmpInst::Predicate pred = cmp->getPredicate();

    if (LoadInst *load = dyn_cast<LoadInst>(lhs)) {
        if (ConstantInt *c = dyn_cast<ConstantInt>(rhs)) {
            var = get_base_ptr(load->getPointerOperand());
            cint = c;
        }
    } else if (LoadInst *load = dyn_cast<LoadInst>(rhs)) {
        if (ConstantInt *c = dyn_cast<ConstantInt>(lhs)) {
            var = get_base_ptr(load->getPointerOperand());
            cint = c;
            pred = CmpInst::getSwappedPredicate(pred);
        }
    } else if (Argument *arg = dyn_cast<Argument>(lhs)) {
        if (ConstantInt *c = dyn_cast<ConstantInt>(rhs)) {
            var = arg;
            cint = c;
        }
    } else if (Argument *arg = dyn_cast<Argument>(rhs)) {
        if (ConstantInt *c = dyn_cast<ConstantInt>(lhs)) {
            var = arg;
            cint = c;
            pred = CmpInst::getSwappedPredicate(pred);
        }
    } else if (ConstantInt *c = dyn_cast<ConstantInt>(rhs)) {
        if (pred_state.val_ranges.count(lhs)) {
            var = lhs;
            cint = c;
        }
    } else if (ConstantInt *c = dyn_cast<ConstantInt>(lhs)) {
        if (pred_state.val_ranges.count(rhs)) {
            var = rhs;
            cint = c;
            pred = CmpInst::getSwappedPredicate(pred);
        }
    }

    if (!var || !cint || !pred_state.val_ranges.count(var)) return pred_state;

    range var_r = pred_state.val_ranges[var];
    int64_t const_val = cint->getSExtValue();
    
    BasicBlock *true_bb = br->getSuccessor(0);
    bool is_true_path = (succ == true_bb);

    if (!is_true_path) pred = CmpInst::getInversePredicate(pred);

    range filter_r = r_top;
    switch (pred) {
        case ICmpInst::ICMP_SGT: 
            if (const_val == INT_MAX) {
                filter_r = r_bot;  
            } else {
                filter_r = range(const_val + 1, INT_MAX); 
            }
            break;
        case ICmpInst::ICMP_SGE: 
            filter_r = range(const_val, INT_MAX); 
            break;
        case ICmpInst::ICMP_SLT: 
            if (const_val == INT_MIN) {
                filter_r = r_bot;  
            } else {
                filter_r = range(INT_MIN, const_val - 1); 
            }
            break;
        case ICmpInst::ICMP_SLE: 
            filter_r = range(INT_MIN, const_val); 
            break;
        case ICmpInst::ICMP_EQ:  
            filter_r = range(const_val, const_val); 
            break;
        case ICmpInst::ICMP_NE:
            if (var_r.get_low() == const_val && var_r.get_high() == const_val) {
                filter_r = r_bot;
            }
            break;
        default: 
            break;
    }
    
    range refined = range::intersect(var_r, filter_r);
    pred_state.val_ranges[var] = refined;
    
    if (refined == r_bot) pred_state.reachable = false;
    
    return pred_state;
}

block_state refine_sw(SwitchInst *sw, BasicBlock *succ, 
                                 block_state pred_state) {
    Value *cond = sw->getCondition();
    Value *var = nullptr;
    
    if (LoadInst *load = dyn_cast<LoadInst>(cond)) {
        var = get_base_ptr(load->getPointerOperand());
    } else if (isa<Argument>(cond) || pred_state.val_ranges.count(cond)) {
        var = cond;
    }
    
    if (!var || !pred_state.val_ranges.count(var)) return pred_state;

    bool is_default = (sw->getDefaultDest() == succ);
    
    if (!is_default) {
        range case_r = r_bot;
        for (auto &case_val : sw->cases()) {
            if (case_val.getCaseSuccessor() == succ) {
                int64_t val = case_val.getCaseValue()->getSExtValue();
                case_r = range::join(case_r, range(val, val));
            }
        }
        range old_r = pred_state.val_ranges[var];
        range refined = range::intersect(old_r, case_r);
        pred_state.val_ranges[var] = refined;
        
        if (refined == r_bot) {
            pred_state.reachable = false;
        }
    } 
    return pred_state;
}

void widen_loop(block_state &pred_state, block_state &in_state, 
                   BasicBlock *pred, BasicBlock *curr,
                   std::set<std::pair<BasicBlock*, BasicBlock*>> &back_edges) {
    
    if (!back_edges.count({pred, curr})) return;
    
    for (auto const& [val, old_r] : in_state.val_ranges) {
        if (pred_state.val_ranges.count(val)) {
            range new_r = pred_state.val_ranges[val];
            
            if (old_r != new_r && old_r != r_bot) {
                bool lower_expanding = (new_r.get_low() < old_r.get_low());
                bool upper_expanding = (new_r.get_high() > old_r.get_high());
                
                if (lower_expanding || upper_expanding) {
                    int new_low = lower_expanding ? INT_MIN : new_r.get_low();
                    int new_high = upper_expanding ? INT_MAX : new_r.get_high();
                    pred_state.val_ranges[val] = range(new_low, new_high);
                }
            }
        }
    }
    
    for (auto const& [alloc, old_arr] : in_state.arr_states) {
        if (pred_state.arr_states.count(alloc)) {
            if (pred_state.arr_states[alloc].default_r != old_arr.default_r ||
                pred_state.arr_states[alloc].elem_map != old_arr.elem_map) {
                pred_state.arr_states[alloc].default_r = r_top;
                pred_state.arr_states[alloc].elem_map.clear();
            }
        }
    }
}

bool needs_check(range r, uint64_t size) {
    if (r == r_bot) return false;
    
    if (size == 0) return true;
    
    if (r != r_top && r.get_low() >= 0 && r.get_high() < (int)size) {
        return false;
    }
    return true;
}

void add_check(GetElementPtrInst *gep, uint64_t size, block_state &state) {
    errs() << "Instrumenting " << *gep << "\n";
    
    IRBuilder<> builder(gep);
    
    auto idx_it = gep->idx_begin();
    ++idx_it;
    Value *idx = idx_it->get();
    
    IntegerType *idx_type = cast<IntegerType>(idx->getType());
    
    Value *zero = ConstantInt::get(idx_type, 0);
    Value *size_val = ConstantInt::get(idx_type, size);
    
    Value *low_check = builder.CreateICmpSGE(idx, zero);
    Value *high_check = builder.CreateICmpSLT(idx, size_val);
    Value *in_bounds = builder.CreateAnd(low_check, high_check);
    
    BasicBlock *curr_bb = gep->getParent();
    Function *fun = curr_bb->getParent();
    BasicBlock *cont_bb = curr_bb->splitBasicBlock(gep, "cont.bb");
    BasicBlock *err_bb = BasicBlock::Create(curr_bb->getContext(), "err.bb", fun, cont_bb);
    
    curr_bb->getTerminator()->eraseFromParent();
    
    builder.SetInsertPoint(curr_bb);
    builder.CreateCondBr(in_bounds, cont_bb, err_bb);
    
    builder.SetInsertPoint(err_bb);
    builder.CreateRet(builder.getInt32(-1));
}

PreservedAnalyses run_pass(Function &F) {
    if (F.getName() != "test") return PreservedAnalyses::all();

    std::map<BasicBlock*, block_state> in_state, out_state;
    std::queue<BasicBlock*> wk;
    std::set<BasicBlock*> in_wk;

    SmallVector<std::pair<const BasicBlock*, const BasicBlock*>, 8> be_vec;
    FindFunctionBackedges(F, be_vec);
    std::set<std::pair<BasicBlock*, BasicBlock*>> back_edges;
    for (const auto& edge : be_vec) {
        back_edges.insert({const_cast<BasicBlock*>(edge.first), 
                           const_cast<BasicBlock*>(edge.second)});
    }

    for (BasicBlock &BB : F) {
        wk.push(&BB);
        in_wk.insert(&BB);
    }

    while (!wk.empty()) {
        BasicBlock *BB = wk.front();
        wk.pop();
        in_wk.erase(BB);

        block_state new_in_state;
        
        if (BB == &F.getEntryBlock()) {
            new_in_state.reachable = true;
            for (Argument &arg : F.args()) {
                new_in_state.val_ranges[&arg] = r_top;
            }
        } else {
            for (BasicBlock *pred_bb : predecessors(BB)) {
                if (!out_state[pred_bb].reachable) continue;
                
                block_state pred_out = out_state[pred_bb];

                if (BranchInst *br = dyn_cast<BranchInst>(pred_bb->getTerminator())) {
                    pred_out = refine_br(br, BB, pred_out);
                } else if (SwitchInst *sw = dyn_cast<SwitchInst>(pred_bb->getTerminator())) {
                    pred_out = refine_sw(sw, BB, pred_out);
                }

                widen_loop(pred_out, in_state[BB], pred_bb, BB, back_edges);
                
                if (pred_out.reachable) {
                    new_in_state.join_state(pred_out);
                }
            }
        }
        
        in_state[BB] = new_in_state;

        block_state old_out_state = out_state[BB];
        block_state current_state = in_state[BB];
        
        if (!current_state.reachable) {
            out_state[BB] = current_state;
            if (old_out_state != current_state) {
                for (BasicBlock *succ_bb : successors(BB)) {
                    if (in_wk.find(succ_bb) == in_wk.end()) {
                        wk.push(succ_bb);
                        in_wk.insert(succ_bb);
                    }
                }
            }
            continue;
        }
        
        for (Instruction &inst : *BB) {
            transfer_inst(inst, current_state, out_state);
        }
        
        out_state[BB] = current_state;

        if (old_out_state != current_state) {
            for (BasicBlock *succ_bb : successors(BB)) {
                if (in_wk.find(succ_bb) == in_wk.end()) {
                    wk.push(succ_bb);
                    in_wk.insert(succ_bb);
                }
            }
        }
    }
    std::vector<GetElementPtrInst*> geps;
    for (BasicBlock &BB : F) {
        for (Instruction &inst : BB) {
            if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(&inst)) {
                geps.push_back(gep);
            }
        }
    }

    bool modified = false;
    
    for (GetElementPtrInst *gep : geps) {
        BasicBlock *BB = gep->getParent();
        
        if (!in_state[BB].reachable) {
            errs() << "Skipped check for " << *gep << " (unreachable)\n";
            continue;
        }
        
        Value *base = gep->getPointerOperand();
        AllocaInst *alloc = dyn_cast<AllocaInst>(base);
        
        if (!alloc) continue;
        
        Type *alloc_type = alloc->getAllocatedType();
        ArrayType *arr_type = dyn_cast<ArrayType>(alloc_type);
        if (!arr_type) continue;
        
        uint64_t arr_size = arr_type->getNumElements();
        block_state state_at_inst = in_state[BB];
        
        for (Instruction &inst : *BB) {
            if (&inst == gep) break;
            
            std::map<BasicBlock*, block_state> temp_out_states = out_state;
            transfer_inst(inst, state_at_inst, temp_out_states);
        }
        
        range idx_r = r_top;
        Value *idx_val = nullptr;
        if (gep->getNumIndices() >= 2) {
            auto idx_it = gep->idx_begin();
            ++idx_it;
            idx_val = idx_it->get();
            idx_r = get_range(idx_val, state_at_inst);
            
            errs() << "  Checking index: " << *idx_val << "\n";
            
            if (state_at_inst.val_ranges.count(idx_val)) {
                errs() << "  Index range: " << state_at_inst.val_ranges[idx_val] << "\n";
            }
            
            if (LoadInst *load = dyn_cast<LoadInst>(idx_val)) {
                Value *ptr = load->getPointerOperand();
                errs() << "  Index loaded from: " << *ptr << "\n";
                if (state_at_inst.val_ranges.count(ptr)) {
                    errs() << "  Ptr range: " << state_at_inst.val_ranges[ptr] << "\n";
                }
            }
            
            errs() << "  Final range: " << idx_r << "\n";
        }

        bool needs_instrument = needs_check(idx_r, arr_size);
        
        if (idx_r == r_bot) {
            errs() << "  Unreachable\n";
        } else if (!needs_instrument) {
            errs() << " Should be safe [" << idx_r.get_low() << "," 
                   << idx_r.get_high() << "] within [0," << (arr_size-1) << "]\n";
        } else {
            errs() << "It needs  check (range: " << idx_r 
                   << ", size: " << arr_size << ")\n";
        }

        if (!needs_instrument) {
            errs() << "Skipped check for " << *gep << "\n";
        } else {
            add_check(gep, arr_size, state_at_inst);
            modified = true;
        }
    }
    
    return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

class ArrayInstrumentationPass : public PassInfoMixin<ArrayInstrumentationPass> {
public:
    static bool isRequired() { return true; }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        return run_pass(F);
    };
};

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "Array InstrumentationPass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "instrument-array-accesses") {
                        FPM.addPass(ArrayInstrumentationPass());
                        return true;
                    }
                    return false;
                });
        }
    };
}
