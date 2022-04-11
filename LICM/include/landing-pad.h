#ifndef __LANDINGPADTRANSFORM_H___
#define __LANDINGPADTRANSFORM_H___

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Analysis/LoopPass.h>

using namespace llvm;
using namespace std;

namespace llvm {
class LandingPadTransform : public LoopPass {
private:
  void updateLatch(BasicBlock *, BasicBlock *);
  void moveCondFromHeaderToPreheader(BasicBlock *, BasicBlock *);
  void removePhiDependencies(BasicBlock *newtest, BasicBlock *header);
  void updatePhiNotInLoop(Loop &loop,
                          SmallVector<Instruction *, 10> &previousPhiUsers,
                          PHINode *from, PHINode *to);
  void unifyPhiAtExit(BasicBlock *newtest, BasicBlock *unifiedExit,
                      BasicBlock *loopexit, BasicBlock *header,
                      BasicBlock *lastBody, Loop *L);

public:
  static char ID;
  LandingPadTransform();
  virtual bool runOnLoop(Loop *L, LPPassManager &LPM) override;
  virtual void getAnalysisUsage(AnalysisUsage &AU) const override;
};
} // namespace llvm
#endif
