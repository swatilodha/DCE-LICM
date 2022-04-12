INC=-I/usr/local/include/ -I ../Dataflow/include/ -I ./include
SOURCES:= $(shell find ../Dataflow/src -type f -name '*.cpp')
OBJECTS:= $(SOURCES:.cpp=.o)

all: deadCodeElimination.so create-tests

create-tests:
	make -C tests/

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) $(INC) -g -O0 -fPIC

../Dataflow/src/%.o: ../Dataflow/src/%.cpp

deadCodeElimination.so: ./src/deadCodeElimination.o $(OBJECTS)
	$(CXX) -dylib -shared $^ -o $@

run-dce-1: all
	opt -enable-new-pm=0 -load ./deadCodeElimination.so -dead-code-elimination ./tests/dce_test1-m2r.bc -o ./tests/dce_test1-opt.bc

run-dce-2: all
	opt -enable-new-pm=0 -load ./deadCodeElimination.so -dead-code-elimination ./tests/dce_test2-m2r.bc -o ./tests/dce_test2-opt.bc

clean:
	rm -f *.o ./*/*.o *~ *.so tests/*.bc tests/*.ll

.PHONY: clean all
