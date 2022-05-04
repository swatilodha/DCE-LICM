INC=-I/usr/local/include/ -I ../Dataflow/include/ -I ./include
SOURCES:= $(shell find ../Dataflow/src -type f -name '*.cpp')
OBJECTS:= $(SOURCES:.cpp=.o)
all: pre.so create-tests

create-tests:
	make -C tests/

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) $(INC) -g -O0 -fPIC

../Dataflow/src/%.o: ../Dataflow/src/%.cpp

./src/pre-support.o: ./src/pre-support.cpp

pre.so: ./src/pre.o ./src/anticipated.o ./src/available.o ./src/postponable.o ./src/used.o ./src/pre-support.o $(OBJECTS)
	$(CXX) -dylib -shared $^ -o $@

test: all
	opt -enable-new-pm=0 -load ./pre.so -pre ./tests/mbenchmark1-m2r.bc -o ./tests/mbenchmark1-opt.bc
	opt -enable-new-pm=0 -load ./pre.so -pre ./tests/mbenchmark2-m2r.bc -o ./tests/mbenchmark2-opt.bc
	opt -enable-new-pm=0 -load ./pre.so -pre ./tests/mbenchmark3-m2r.bc -o ./tests/mbenchmark3-opt.bc
	opt -enable-new-pm=0 -load ./pre.so -pre ./tests/mbenchmark4-m2r.bc -o ./tests/mbenchmark4-opt.bc
	opt -enable-new-pm=0 -load ./pre.so -pre ./tests/mbenchmark5-m2r.bc -o ./tests/mbenchmark5-opt.bc
	llvm-dis ./tests/mbenchmark1-opt.bc
	llvm-dis ./tests/mbenchmark2-opt.bc
	llvm-dis ./tests/mbenchmark3-opt.bc
	llvm-dis ./tests/mbenchmark4-opt.bc
	llvm-dis ./tests/mbenchmark5-opt.bc

clean:
	rm -f *.o ./*/*.o *~ *.so tests/*.bc tests/*.ll

.PHONY: clean all
