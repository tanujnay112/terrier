/** @file slp.cpp
 *
 *  @author Prithvi Gudapati
 *  @author Tanuj Nayak
 *
 *  @brief Implementation of a superword parallelism auto-vectorization pass
 *
 *
 */


#include <assert.h>
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/ADT/APInt.h"

#include "execution/vm/745SLPPass.h"


#define MAX_VEC_REG_SIZE 256
#define MIN_VEC_REG_SIZE 128
#define MIN_VEC_SIZE       2
#define VEC_SIZE           4


using namespace llvm;

//static bool isValidElementType(Type *Ty) {
//  return VectorType::isValidElementType(Ty) && !Ty->isX86_FP80Ty() &&
//      !Ty->isPPC_FP128Ty();
//}


typedef struct DAGNodeT  {
  SmallVector<Value*, 8> left_values;
  SmallVector<Value*, 8> right_values;
  SmallVector<Value*, 8> instructions;
  int prev_loc;
  Instruction::BinaryOps opcode;
} DAGNode;


class SLP : public FunctionPass {
 public:
  static char ID;

  SLP() : FunctionPass(ID) { }

  virtual bool runOnFunction(Function& F) {
    SE = &getAnalysis<ScalarEvolutionWrapperPass>().getSE();
    DL = &F.getParent()->getDataLayout();
    // StoreMap.clear();
    // GEPMap.clear();
    outs() << "Function Name: " << F.getName() << "\t";
    outs() << "Number of Basic Blocks: " << F.getBasicBlockList().size() << "\t";
    outs() << "Number of Instructions:" << F.getInstructionCount() << "\n";

    MapVector<BasicBlock*, SmallVector<Value *, 8>> seed_map = findSeedInstructions(&F);

    /** TODO: Loop through entries in seed_map. */
    /** TODO: Build a tree and vectorize for each one .*/

    // for(auto &bb : F){
    //     /** TODO: Vectorize each basic block. */


    //     /** Get the seed instructions. */
    //     MapVector<BasicBlock*, ArrayRef<Value *>> seed_map

    //     // outs() << "Found instructions of length" << it->second.size() << "\n";

    //     // for (auto it = seed_list.begin(); it != seed_list.end(); it++) {
    //     //     /** Find store groups of size greater than 2. */
    //     //     // if (it->second.size() < 2)
    //     //     //     continue;

    //     //     outs() << "Found store group of size " << it->second.size() << "\n";
    //     // }
    // }

    for (auto &bb : F) {
      // printAllUseDefChains(&bb);
      /** Group seed instructions by operand type. */
      DenseMap<unsigned, SmallVector<Value *, 8>> seeds_by_op;
      for(auto val : seed_map[&bb]) {
        if (auto I = dyn_cast<Instruction>(val)) {
          seeds_by_op[I->getOpcode()].push_back(val);
        }
      }
      for (auto it = seeds_by_op.begin(); it != seeds_by_op.end(); it++) {
        if ((it->second).size() >= MIN_VEC_SIZE) {
          SmallVector<Value *, 8> cur_seed_set;
          for (auto val : it->second) {
            /** TODO: Maybe compute the VEC_SIZE based on the type of the value? */
            cur_seed_set.push_back(val);

            if (cur_seed_set.size() == VEC_SIZE) {
              vectorizeSeedSet(&bb, cur_seed_set);

              cur_seed_set.clear();
            }
          }
          if (cur_seed_set.size() >= MIN_VEC_SIZE) {
            vectorizeSeedSet(&bb, cur_seed_set);
          }
        }
      }
    }

    outs() << "\n \n";
    return false;
  };

  virtual void getAnalysisUsage(AnalysisUsage& AU) const {
    FunctionPass::getAnalysisUsage(AU);
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.setPreservesCFG();
  }


 private:

  void vectorizeSeedSet(BasicBlock *bb, SmallVector<Value*, 8> &seed_set) {
    outs() << "Basic Block ";
    bb->printAsOperand(outs(), false);
    outs() << ": Vectorization Tree\n";
    bb->printAsOperand(outs(), false);
    SmallVector<DAGNode, 8> DAG= buildVectorizationDAG(bb, seed_set);

    outs() << "Basic Block ";
    bb->printAsOperand(outs(), false);
    outs() << ": Vector Instruction Scheduling\n";
    scheduleVectorizationDAG(bb, DAG);
  }

  MapVector<BasicBlock *, SmallVector<Value *, 8>> findSeedInstructions(Function *F) {
    SmallVector<Value*, 8> arg_vector;
    for (auto it = F->arg_begin(); it != F->arg_end(); it++) {
      arg_vector.push_back(it);
      for(auto U : it->users()){  // U is of type User*
        if (auto I = dyn_cast<ExtractValueInst>(U)){
          outs() << "Extract Element User \n";
          arg_vector.push_back(I);
        }
      }
    }

    outs() << "Found " << arg_vector.size() << " args \n";

    MapVector<BasicBlock *, SmallVector<Value*, 8>> seed_map_vec;
    for (auto arg : arg_vector) {
      for (auto U : arg->users()) {
        if (auto I = dyn_cast<Instruction>(U)){
          if (!isa<ExtractValueInst>(I)) {
            seed_map_vec[I->getParent()].push_back(I);
          }
        }
      }
    }

    for (auto it = seed_map_vec.begin(); it != seed_map_vec.end(); it++) {
      outs() << "Basic Block ";
      (it->first)->printAsOperand(outs(), false);
      outs() << ": Found " << it->second.size() << " seed instructions\n";
      for (auto val : it->second) {
        if (auto I = dyn_cast<Instruction>(val)){
          outs() << "\t";
          I->print(outs());
          outs() << "\n";
        }
      }
      outs() << "\n";
    }


    return seed_map_vec;
  }


  void printAllUseDefChains(BasicBlock *bb) {
    outs() << "Basic Block ";
    bb->printAsOperand(outs(), false);
    outs() << "Printing all use def chains \n";
    for (auto &I : *bb) {
      SetVector<Instruction*> use_def_chain;
      I.print(outs());
      outs() << "\n";
      getUseDefChain(bb, use_def_chain, &I);
      for (auto instr : use_def_chain) {
        outs() << "\t";
        instr->print(outs());
        outs() << "\n";
      }
    }
  }


  void getUseDefChain(BasicBlock *bb, SetVector<Instruction*> &chain, Instruction* inst) {
    for(size_t i = 0; i < inst->getNumOperands(); i++) {
      auto operand = inst->getOperand(i);
      if (auto parent_inst = dyn_cast<Instruction>(operand)) {
        if (parent_inst->getParent() == bb) {
          chain.insert(parent_inst);
          getUseDefChain(bb, chain, parent_inst);
        }
      }
    }
  }


  bool canSchedule(DAGNode &Node, DenseSet<Instruction*> &prev_insts, Instruction *inst) {
//    const auto I = inst;

    bool can_schedule = true;
    if (Node.prev_loc == 1 || Node.prev_loc == -1) {
      for (auto val : Node.left_values) {
        if (auto I2 = dyn_cast<Instruction>(val)) {
          if (prev_insts.count(I2) != 1) {
            /**
             * If the dependent instruction occurs after.
             * Can't schedule the vectorized instruction.
             */
            can_schedule = false;
          }
        }
      }
    }

    if (Node.prev_loc == 0 || Node.prev_loc == -1) {
      for (auto val : Node.right_values) {
        if (const auto I2 = dyn_cast<Instruction>(val)) {
          if (prev_insts.count(I2) != 1) {
            /**
             * If the dependent instruction occurs after.
             * Can't schedule the vectorized instruction.
             */
            can_schedule = false;
          }
        }
      }
    }

    return can_schedule;
  }


  Value *insertInsertElems(SmallVector<Value*, 8> &values, Instruction *inst) {
    Value* initVec = nullptr;
    /** Insert the actual values into the initial vector for ConstantInts. */
    /** TODO: Maybe can do this more cleanly by using templates. */
    if (values.size() > 0) {
      if (auto intType = dyn_cast<IntegerType>(values.front()->getType())) {
//        unsigned bitWidth = intType->getBitWidth();
        switch (intType->getBitWidth()) {
          case 8: {
            std::vector<uint8_t> vec_vals;
            for (auto val : values) {
              if (auto const_val = dyn_cast<ConstantInt>(val)) {
                vec_vals.push_back((uint8_t)(const_val->getZExtValue()));
              } else {
                vec_vals.push_back(0);
              }
            }
            ArrayRef<uint8_t> vec_vals_ref(vec_vals);
            initVec = ConstantDataVector::get(values.front()->getContext(), vec_vals_ref);
            break;
          }
          case 16: {
            std::vector<uint16_t> vec_vals;
            for (auto val : values) {
              if (auto const_val = dyn_cast<ConstantInt>(val)) {
                vec_vals.push_back((uint16_t)(const_val->getZExtValue()));
              } else {
                vec_vals.push_back(0);
              }
            }
            ArrayRef<uint16_t> vec_vals_ref(vec_vals);
            initVec = ConstantDataVector::get(values.front()->getContext(), vec_vals_ref);
            break;
          }
          case 32: {
            std::vector<uint32_t> vec_vals;
            for (auto val : values) {
              if (auto const_val = dyn_cast<ConstantInt>(val)) {
                vec_vals.push_back((uint32_t)(const_val->getZExtValue()));
              } else {
                vec_vals.push_back(0);
              }
            }
            ArrayRef<uint32_t> vec_vals_ref(vec_vals);
            initVec = ConstantDataVector::get(values.front()->getContext(), vec_vals_ref);
            break;
          }
          case 64: {
            std::vector<uint64_t> vec_vals;
            for (auto val : values) {
              if (auto const_val = dyn_cast<ConstantInt>(val)) {
                vec_vals.push_back((uint64_t)(const_val->getZExtValue()));
              } else {
                vec_vals.push_back(0);
              }
            }
            ArrayRef<uint64_t> vec_vals_ref(vec_vals);
            initVec = ConstantDataVector::get(values.front()->getContext(), vec_vals_ref);
            break;
          }
          default:
            assert(false);
        }
      } else {
        initVec = ConstantDataVector::getSplat(values.size(), ConstantFP::get(intType, 0.0));
      }
    }

    /** Insert InsertElements for all non-ConstantInt values. */
    Value* curVec = initVec;
    int index = 0;
    for (auto val : values) {
      if (!isa<ConstantInt>(val)) {
        curVec = InsertElementInst::Create(
            curVec, val, ConstantInt::get(Type::getInt64Ty(val->getContext()), index), "", inst
        );
      }
      index++;
    }

    return curVec;
  }


  Instruction *insertDAGVectorInst(DAGNode &Node, Instruction *inst, Value *prev_vec) {
    Value *left_vec = nullptr;
    Value *right_vec = nullptr;

    if (Node.prev_loc == 1 || Node.prev_loc == -1) {
      left_vec = insertInsertElems(Node.left_values, inst);
    } else {
      left_vec = prev_vec;
    }

    if (Node.prev_loc == 0 || Node.prev_loc == -1) {
      right_vec = insertInsertElems(Node.right_values, inst);
    } else {
      right_vec = prev_vec;
    }

    return BinaryOperator::Create(Node.opcode, left_vec, right_vec, Twine(), inst);
  }



  void scheduleVectorizationDAG(BasicBlock *bb, SmallVector<DAGNode, 8> &DAG) {
    /**
     *  TODO: Deal with the issues of things of something in the
     *  use-def chain of one lane coming after the use of another
     *  another lane. Though this may not be an issue with our
     *  specific workload.
     */
    DenseSet<Instruction*> prev_instructions;

    Instruction *cur_inst = bb->getFirstNonPHI();
    cur_inst->print(outs());
    outs() << "\n";

    bool is_last_instruction = (cur_inst == bb->getTerminator());

    SmallVector<Instruction*, 8> inst_to_remove;

    Instruction *last_vec = nullptr;

    int index = 1;
    for (auto Node : DAG) {
//      bool scheduled_vector_inst = false;

      while(!is_last_instruction && !canSchedule(Node, prev_instructions, cur_inst)) {
        outs() << "Skip\n";
        prev_instructions.insert(cur_inst);
        cur_inst = cur_inst->getNextNonDebugInstruction();
        is_last_instruction = (cur_inst == bb->getTerminator());
        cur_inst->print(outs());
        outs() << "\n";
      }

      outs() << "Schedule an instruction here \n";

      last_vec = insertDAGVectorInst(Node, cur_inst, last_vec);

      if (index != DAG.size()) {
        for (auto I : Node.instructions) {
          auto inst = dyn_cast<Instruction>(I);
          assert(inst != nullptr);
          inst_to_remove.push_back(inst);
        }
      }

      index++;
    }

    if (last_vec) {
      DenseMap<Value *, Value *> old_to_new;
      int i = 0;
      for (auto val : DAG.back().instructions) {
        Value *extracted = ExtractElementInst::Create(last_vec, ConstantInt::get(Type::getInt64Ty(val->getContext()), i), "", cur_inst);
        val->replaceAllUsesWith(extracted);
        auto inst = dyn_cast<Instruction>(val);
        assert(inst != nullptr);
        inst_to_remove.push_back(inst);
        i++;
      }
    }

    // outs() << "Instructions to Remove \n";
    for (auto I : inst_to_remove) {
      I->eraseFromParent();
      // I->print(outs());
      // outs() << "\n";
    }
  }


  SmallVector<DAGNode, 8> buildVectorizationDAG(BasicBlock *bb, SmallVector<Value *, 8> instructions) {
    SmallVector<DAGNode, 8> DAG;

    if (instructions.size() == 0) return DAG;

    bool end_vectorization = false;
    DenseMap<Value*, Value*> prev_instructions;
    do {
      unsigned opcode = 0;
      bool has_intructions = false;

      /** Checking if the instructions are vectorizable. */
      for (auto val : instructions) {
        if (auto I = dyn_cast<Instruction>(val)) {
          /** Instruction needs to be in the BasicBlock of interest. */
          if (I->getParent() != bb) {
            end_vectorization = true;
            break;
          }

          /** All the instructions need to have the same opcode. */
          if (!has_intructions) {
            opcode = I->getOpcode();
            has_intructions = true;
          } else if (opcode != I->getOpcode()) {
            end_vectorization = true;
            break;
          }

          /**
           *  The instructions need to be independent of one another.
           *  We check this by making sure that no instruction is in another
           *  instructions use-def chain.
           */
          SetVector<Instruction*> use_def_chain;
          getUseDefChain(bb, use_def_chain, I);
          bool dependent = false;
          for (auto val2 : instructions) {
            if (auto I2 = dyn_cast<Instruction>(val2)) {
              if (I2 != I && use_def_chain.count(I2) > 0) {
                dependent = true;
                break;
              }
            }
          }

          if (dependent) {
            end_vectorization = true;
            break;
          }
        } else {
          end_vectorization = true;
          break;;
        }
      }

      if (!end_vectorization) {
        switch (opcode) {
          case Instruction::Add:
          case Instruction::FAdd:
          case Instruction::Sub:
          case Instruction::FSub:
          case Instruction::Mul:
          case Instruction::FMul:
          case Instruction::UDiv:
          case Instruction::SDiv:
          case Instruction::FDiv:
          case Instruction::URem:
          case Instruction::SRem:
          case Instruction::FRem:
          case Instruction::Shl:
          case Instruction::LShr:
          case Instruction::AShr:
          case Instruction::And:
          case Instruction::Or:
          case Instruction::Xor: {
            /** Check if the previous operands are in the right locations */
            int prev_loc = -1;
            DenseMap<Instruction*, Value*> left_op;
            DenseMap<Instruction*, Value*> right_op;
            bool op_loc_mismatch = false;
            for (auto val : instructions) {
              auto I = dyn_cast<Instruction>(val);
              if (prev_instructions.size() > 0) {
                if (I->isCommutative()) {
                  /** If commutative make the prev operation always the left. */
                  if (prev_instructions[I] == I->getOperand(0)) {
                    left_op[I] = I->getOperand(0);
                    right_op[I] = I->getOperand(1);
                  } else {
                    left_op[I] = I->getOperand(1);
                    right_op[I] = I->getOperand(0);
                  }
                  prev_loc = 0;
                } else {
                  /** Find where the previous instruction is and make sure it matches the other instructions. */
                  int loc = prev_instructions[I] == I->getOperand(0) ? 0 : 1;
                  if (prev_loc == -1) {
                    prev_loc = loc;
                  } else if (prev_loc != loc) {
                    op_loc_mismatch = true;
                    break;
                  }
                  left_op[I] = I->getOperand(0);
                  right_op[I] = I->getOperand(1);                                       }
              } else {
                left_op[I] = I->getOperand(0);
                right_op[I] = I->getOperand(1);
              }
            }

            if (op_loc_mismatch) {
              end_vectorization = true;
              break;
            }


            /** Make sure there is only 1 use. */
            for (auto val : instructions) {
              auto I = dyn_cast<Instruction>(val);

              if (I->getNumUses() != 1) {
                end_vectorization = true;
              }
            }

            DAGNode curNode;
            BinaryOperator *op = dyn_cast<BinaryOperator>(instructions.front());
            curNode.opcode = op->getOpcode();
            curNode.prev_loc = prev_loc;

            /** Add this node to the tree. */
            for (auto val : instructions) {
              auto I = dyn_cast<Instruction>(val);
              /** They are all binary ops. */
              curNode.left_values.push_back(left_op[I]);
              curNode.right_values.push_back(right_op[I]);
            }

            for (auto val : instructions) {
              auto I = dyn_cast<Instruction>(val);
              curNode.instructions.push_back(I);
            }

            if (!end_vectorization) {
              /** Update the previous instructions and the current instructions. */
              SmallVector<Value*, 8> instruction_vec = SmallVector<Value*, 8>();
              prev_instructions.clear();
              for (auto val : instructions) {
                auto I = dyn_cast<Instruction>(val);
                prev_instructions[*(I->users().begin())] = I;
                instruction_vec.push_back(*(I->users().begin()));
              }
              instructions.clear();
              for (auto val: instruction_vec) {
                instructions.push_back(val);
              }
            }

            DAG.push_back(curNode);
            break;
          }
          default:
            end_vectorization = true;
            break;
        }
      }

    } while(!end_vectorization);

    for(auto curNode : DAG) {
      outs() << "Operand: " << curNode.opcode << "\n";

      outs() << "Prev Loc: " << curNode.prev_loc << "\n";

      outs() << "Instructions: \n";
      for (auto val : curNode.instructions) {
        outs() << "\t";
        val->print(outs());
        outs() << "\n";
      }

      outs() << "Left Values: \n";

      /** Print Left Values. */
      for (auto val : curNode.left_values) {
        outs() << "\t";
        val->print(outs());
        outs() << "\n";
      }

      outs() << "Right Values: \n";

      /** Print Right Values. */
      for (auto val : curNode.right_values) {
        outs() << "\t";
        val->print(outs());
        outs() << "\n";
      }
    }

    return DAG;
  }

  const DataLayout *DL;
  ScalarEvolution *SE;
};


char SLP::ID = 0;
RegisterPass<SLP> X("SLP", "15745 Project SLP");

FunctionPass *llvm::create745SLPPass() {
  return new SLP();
}