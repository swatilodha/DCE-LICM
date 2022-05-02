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

clean:
	rm -f *.o ./*/*.o *~ *.so tests/*.bc tests/*.ll

.PHONY: clean all
