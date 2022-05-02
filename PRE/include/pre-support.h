#ifndef __ANTICIPATED_SUPPORT_H__
#define __ANTICIPATED_SUPPORT_H__

#include <string>
#include <vector>

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

using namespace std;

string getShortValueName(Value *v);

class Expression {
public:
  Value *operand1;
  Value *operand2;
  Instruction::BinaryOps op;
  Expression() {}
  Expression(Instruction *I);
  bool operator==(const Expression &e2) const;
  bool operator<(const Expression &e2) const;
  string toString() const;
};

void printSet(vector<Expression> exps);
} // namespace llvm

#endif