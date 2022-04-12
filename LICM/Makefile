INC=-I/usr/local/include/ -I ../Dataflow/include/ -I ./include/
SOURCES:= $(shell find ../Dataflow/src -type f -name '*.cpp')
OBJECTS:= $(SOURCES:.cpp=.o)

all: licm.so landing-pad.so create-tests

create-tests:
	make -C tests/

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) $(INC) -g -O0 -fPIC

../Dataflow/src/%.o: ../Dataflow/src/%.cpp

licm.so: ./src/licm.o $(OBJECTS)
	$(CXX) -dylib -shared $^ -o $@

landing-pad.so: ./src/landing-pad.o $(OBJECTS)
	$(CXX) -dylib -shared $^ -o $@

run-licm-1: all
	opt -enable-new-pm=0 -load ./landing-pad.so -landing-pad ./tests/benchmark1-m2r.bc -o ./tests/benchmark1-m2r-lpt.bc
	opt -enable-new-pm=0 -load ./licm.so -loop-invariant-code-motion ./tests/benchmark1-m2r-lpt.bc -o ./tests/benchmark1-m2r-licm.bc

run-licm-2: all
	opt -enable-new-pm=0 -load ./landing-pad.so -landing-pad ./tests/benchmark2-m2r.bc -o ./tests/benchmark2-m2r-lpt.bc
	opt -enable-new-pm=0 -load ./licm.so -loop-invariant-code-motion ./tests/benchmark2-m2r-lpt.bc -o ./tests/benchmark2-m2r-licm.bc


run-licm-3: all
	opt -enable-new-pm=0 -load ./landing-pad.so -landing-pad ./tests/benchmark3-m2r.bc -o ./tests/benchmark3-m2r-lpt.bc
	opt -enable-new-pm=0 -load ./licm.so -loop-invariant-code-motion ./tests/benchmark3-m2r-lpt.bc -o ./tests/benchmark3-m2r-licm.bc


clean:
	rm -f *.o ./*/*.o *~ *.so tests/*.bc tests/*.ll

.PHONY: clean all