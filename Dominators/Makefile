INC=-I/usr/local/include/ -I ../Dataflow/include/ -I ./include
SOURCES:= $(shell find ../Dataflow/src -type f -name '*.cpp')
OBJECTS:= $(SOURCES:.cpp=.o)
all: dominators.so create-tests

create-tests:
	make -C tests/

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) $(INC) -g -O0 -fPIC

../Dataflow/src/%.o: ../Dataflow/src/%.cpp

dominators.so: ./src/dominators.o $(OBJECTS)
	$(CXX) -dylib -shared $^ -o $@

run-dom: all
	opt -enable-new-pm=0 -load ./dominators.so -dominators ./tests/benchmark1-m2r.bc -o ./tests/benchmark1-m2r.bc

clean:
	rm -f *.o ./*/*.o *~ *.so tests/*.bc

.PHONY: clean all
