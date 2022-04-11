#include "dataflow.h"

using namespace std;

namespace llvm {

/* This function initializes the post-order and reverse post-order traversal
 * vectors for the framework.
 */
void Dataflow::populateTraversal(Function &F) {
  stack<BasicBlock *> rpoStack;
  po_iterator<BasicBlock *> start = po_begin(&F.getEntryBlock());
  po_iterator<BasicBlock *> end = po_end(&F.getEntryBlock());
  while (start != end) {
    poTraversal.push_back(*start);
    rpoStack.push(*start);
    ++start;
  }

  while (!rpoStack.empty()) {
    rpoTraversal.push_back(rpoStack.top());
    rpoStack.pop();
  }
}

// Initializes the predecessors and successors for a given block.
void Dataflow::populateEdges(BasicBlock *BB, struct bbProps *props) {
  for (BasicBlock *predBlocks : predecessors(BB)) {
    props->pBlocks.push_back(predBlocks);
  }
  for (BasicBlock *succBlocks : successors(BB)) {
    props->sBlocks.push_back(succBlocks);
  }
}

// Initializes the input and output BitVector of each basic block, depending on
// the Pass direction and the block type.
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

/* This function is used to initialize the entire state of the Dataflow object.
 * This involves creating an entry for each BasicBlock in the result map. We
 * also initialize the bbProps struct associated with each BasicBlock, with the
 * help of the info map provided by the client. We initialize the predecessors
 * and successors of each block, and assign the boundary and initial conditions.
 */
void Dataflow::initialize(Function &F,
                          map<BasicBlock *, struct bbInfo *> infoMap) {
  BitVector empty(domainSize, false); // Empty Set

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

  initialize(F, infoMap);
  populateTraversal(F);

  int iter = 0;
  vector<BasicBlock *> traversal =
      (dir == FORWARD) ? poTraversal : rpoTraversal;

  // Begin Dataflow iteration.
  while (!converged) {
    for (BasicBlock *BB : traversal) {
      prevOutput[BB] = result[BB]->bbOutput; // Collect previous outputs of each
                                             // block before iterating.
      vector<BitVector> meetInputs;

      // Collect inputs for the meet function, based on the pass direction. If
      // it is a Forward pass, the output of predecessor blocks will be treated
      // as inputs to the meet function. If it is a Backward pass, the input of
      // successor blocks will be treated as inputs to the meet function.
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

        // Based on the Pass direction, the output of the meet function is
        // assigned to either BasicBlock's input or output.
        if (dir == FORWARD) {
          result[BB]->bbInput = meet;
        } else if (dir == BACKWARD) {
          result[BB]->bbOutput = meet;
        }
      }

      // Apply the trasnfer function to the block.
      transferFn(result[BB]);
    }

    // Check if the algorithm has converged, by comparing the values of all
    // previous outputs, and the current outputs.
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