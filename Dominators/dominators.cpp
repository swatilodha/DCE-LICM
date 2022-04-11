// ECE/CS 5544 S22 Assignment 3: dominators.cpp
// Group: Swati Lodha, Abhijit Tripathy

////////////////////////////////////////////////////////////////////////////////

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "dataflow.h"

using namespace llvm;
using namespace std;

namespace llvm {

class Dominators : public FunctionPass {
public:
  static char ID;
  Dominators() : FunctionPass(ID) {}

  virtual bool runOnFunction(Function &F) {
 
 	//setup domain for Dominators
    setupDomain(F);

	//intialize the gen set and kill set
    populateInfoMap(F);

	//boundary and init conditions
    BitVector boundaryCond(domain.size(), false);
    BitVector initCond(domain.size(), true);

	//Direction of dominator analysis is forward
    Analysis *dfa = new Analysis(domain.size(), FORWARD, boundaryCond, initCond);
    //run the dataflow pass
    dfa->run(F, infoMap);
    
    // Print the dominator results
    printResults(dfa->result);
    
    //Print immediate dominators 
    printImmediateDominators(dfa->result, F);

    return false;
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<LoopInfoWrapperPass>();
  }

private:
  map<BasicBlock *, struct bbInfo *> infoMap;
  map<string, int> domainToBitMap;
  map<int, string> bitToDomainMap;
  vector<string> domain;
  map<BasicBlock*, BasicBlock*> imDom;

  /**
  * This function prints the result for this pass
  */
  void printResults(map<BasicBlock *, struct bbProps *> dfa) {
    LoopInfo &loop_info = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    int ct = 0;
    for (Loop *loop : loop_info) {
      outs() << "Loop " << ct << " :\n";
      for (BasicBlock *bb : loop->getBlocksVector()) {
        outs() << "Basic Block : " << bb->getName() << " :\n";
        struct bbProps *props = dfa[bb];
        vector<string> output;
        for (int idx = 0; idx < props->bbOutput.size(); ++idx) {
          if (props->bbOutput[idx]) {
            output.push_back(bitToDomainMap[idx]);
          }
        }
        
        //print the set
        bool first = true;
		outs() << "{";
		for (string str : output) {
		  if (!first) {
		    outs() << ", ";
		  } else {
		    first = false;
		  }
		  outs() << str;
		}
		outs() << "}\n";
    
        //print the next line    
        outs() << "\n";
      }
    }
  }
  
  /**
  * According to the definition: Each node 'n' has a unique immedite dominator 'm', if 
  * later is the last dominator of node 'n' on any path from entry to node 'n'.
  */
  void printImmediateDominators(map<BasicBlock *, struct bbProps *> dfa, Function &F) {
  	  //iterate over all basic blocks
      for (BasicBlock &blk : F) {
      	BasicBlock* A = &blk;
      	
      	for (BasicBlock &blk1 : F) {
      		BasicBlock* B = &blk1;	
      		//continue if two basic blocks are same
      		if (A == B)	continue; 
      		//get index of this basic block
      		int idx = domainToBitMap[blk1.getName().str()];
      		//if not dominates, then continue
      		if (dfa[A]->bbInput[idx] == false)	continue;
      		
      		bool isImmediateDom = true;
      		
      		for (BasicBlock &blk2 : F) {
      			BasicBlock* C = &blk2;
      			//if this basic block is same as A or B, continue
      			if (C == A || C == B)	continue;
      			
      			int idx1 = domainToBitMap[blk2.getName().str()];
      			//if C is immediate dominator, then B can't be 
      			//so break the loop
      			if (dfa[C]->bbInput[idx] && dfa[A]->bbInput[idx1]) {
      				isImmediateDom = false;
      				break;
      			}
      		}
      		
      		//store the immediate dominator in the map
      		if (isImmediateDom) {
      			imDom[A] = B;
      		}
      	}
      	
      }
      
      //print the block and immediate dominator block to out
      errs() << "New Loop: \n";
      for (auto& [k, v]: imDom) {
          errs() << k->getName().str() << " immediate dominator is: " << v->getName().str() << "\n";
      }
  }
  

 /**
  * This function populates the map we maintain to
  * keep track of the block and their respective indices.
  * It also initializes the gen and kill set to empty
  * and size of the bit vector is same as domain size. 
  */
  void populateInfoMap(Function &F) {
    BitVector empty(domain.size(), false);
    int idx = 0;

    for (string blk : domain) {
      domainToBitMap[blk] = idx;
      bitToDomainMap[idx] = blk;
      ++idx;
    }

    for (BasicBlock &BB : F) {
      struct bbInfo *info = new bbInfo();
      //block reference
      info->ref = &BB;
      //gen set
      info->genSet = empty;
      info->genSet.set(domainToBitMap[BB.getName().str()]);
      //kill set
      info->killSet = empty;
      //append to the map
      infoMap[&BB] = info;
    }
  }
  
  /**
  * This function sets up the domain for the
  * Dominator pass. We push the names of all
  * the basic blocks in the CFG
  */
  void setupDomain(Function &F) {
  	for (BasicBlock &BB : F) {
        domain.push_back(BB.getName().str());
    }
  }

  class Analysis : public Dataflow {
  public:
    Analysis(int domainSize, enum passDirection dir, BitVector boundaryCond,
             BitVector initCond)
        : Dataflow(domainSize, dir, boundaryCond, initCond) {}

	/**
	* Transfer function for Dominators pass is 
	* F(x) = X U {B}, where B is the basic block
	*/
    virtual void transferFn(struct bbProps *props) {
      BitVector tmp = props->bbInput;
      props->bbInput |= props->genSet;
      props->bbOutput = props->bbInput;
      props->bbInput = tmp;
    }

	/**
	* Meet operator for the Dominator pass is INTERSECTION
	*/
    virtual BitVector meetFn(vector<BitVector> inputs) {
      size_t _sz = inputs.size();
      BitVector result = inputs[0];
      for (int itr = 1; itr < _sz; ++itr) {
        result &= inputs[itr];
      }
      return result;
    }
  };
};

char Dominators::ID = 0;
RegisterPass<Dominators> X("dominators", "ECE/CS 5544 Dominators Pass");
} // namespace llvm

