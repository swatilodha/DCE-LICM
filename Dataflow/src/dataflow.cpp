#include "dataflow.h"

using namespace std;

namespace llvm {

void Dataflow::populateTraversal(Function &F) {
  stack<BasicBlock *> rpo_stack;
  po_iterator<BasicBlock *> start = po_begin(&F.getEntryBlock());
  po_iterator<BasicBlock *> end = po_end(&F.getEntryBlock());
  while (start != end) {
    poTraversal.push_back(*start);
    rpo_stack.push(*start);
    ++start;
  }

  while (!rpo_stack.empty()) {
    rpoTraversal.push_back(rpo_stack.top());
    rpo_stack.pop();
  }
}

void Dataflow::populateEdges(BasicBlock *BB, struct bbProps *props) {
  for (BasicBlock *pred_blocks : predecessors(BB)) {
    props->pBlocks.push_back(pred_blocks);
  }
  for (BasicBlock *succ_blocks : successors(BB)) {
    props->sBlocks.push_back(succ_blocks);
  }
}

void Dataflow::initializeBlocks(struct bbProps *block) {
  if (dir == FORWARD) {
    if (block->type == ENTRY)
      block->bbInput = boundaryCond;
    block->bbOutput = initCond;
  } else if (dir == BACKWARD) {
    if (block->type == EXIT)
      block->bbOutput = boundaryCond;
    block->bbInput = initCond;
  }
}

void Dataflow::initializeDfa(Function &F,
                             map<BasicBlock *, struct bbInfo *> infoMap) {
  BitVector empty(domainSize, false);

  for (BasicBlock &BB : F) {
    struct bbProps *props = new bbProps();
    props->ref = &BB;
    props->bbInput = empty;
    props->bbOutput = empty;

    // Set GenSet and KillSet
    struct bbInfo *info = infoMap[&BB];
    props->genSet = info->genSet;
    props->killSet = info->killSet;

    // Initialize predecessors and successors for each BasicBlock
    populateEdges(&BB, props);

    // Determine block type - <ENTRY, EXIT, REGULAR>
    if (&BB == &F.getEntryBlock()) {
      props->type = ENTRY;
    } else {
      props->type = REGULAR;
    }
    for (Instruction &II : BB) {
      Instruction *I = &II;
      if (isa<ReturnInst>(I)) {
        props->type = EXIT;
      }
    }
    result[&BB] = props;
  }

  map<BasicBlock *, struct bbProps *>::iterator it = result.begin();
  struct bbProps *blk;

  // Set initial and boundary conditions for each BasicBlock
  while (it != result.end()) {
    blk = result[it->first];
    initializeBlocks(blk);
    ++it;
  }
}

void Dataflow::run(Function &F, map<BasicBlock *, struct bbInfo *> infoMap) {
  bool converged = false;
  map<BasicBlock *, BitVector> prevOutput;
  initializeDfa(F, infoMap);
  populateTraversal(F);
  int iter = 0;
  vector<BasicBlock *> traversal =
      (dir == FORWARD) ? poTraversal : rpoTraversal;
  while (!converged) {
    for (BasicBlock *BB : traversal) {
      prevOutput[BB] = result[BB]->bbOutput;
      vector<BitVector> meetInputs;
      // Apply meet function based on
      if (dir == FORWARD) {
        for (BasicBlock *p : result[BB]->pBlocks) {
          meetInputs.push_back(result[p]->bbOutput);
        }
      } else if (dir == BACKWARD) {
        for (BasicBlock *s : result[BB]->sBlocks) {
          meetInputs.push_back(result[s]->bbInput);
        }
      }
      if (!meetInputs.empty()) {
        BitVector meet = meetFn(meetInputs);
        if (dir == FORWARD) {
          result[BB]->bbInput = meet;
        } else if (dir == BACKWARD) {
          result[BB]->bbOutput = meet;
        }
      }
      transferFn(result[BB]);
    }
    converged = true;
    for (map<BasicBlock *, BitVector>::iterator it = prevOutput.begin();
         it != prevOutput.end(); ++it) {
      if ((result[it->first]->bbOutput) != it->second) {
        converged = false;
        break;
      }
    }
    ++iter;
  }
  // outs() << "DFA converged after " << iter << " iterations\n";
}
} // namespace llvm