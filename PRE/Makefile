INC=-I/usr/local/include/
# all: liveness.so available.so reaching.so
all: sample-pass.so make_tests

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) $(INC) -g -O0 -fPIC

# dataflow.o: dataflow.cpp dataflow.h

# available-support.o: available-support.cpp available-support.h

# reaching-support.o: reaching-support.cpp reaching-support.h

# %.so: %.o dataflow.o available-support.o reaching-support.o

make_tests: test1.c
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c test1.c -o test1.bc
	opt -mem2reg test1.bc -o test1-m2r.bc
	llvm-dis test1-m2r.bc


%.so: %.o
	$(CXX) -dylib -shared $^ -o $@

clean:
	rm -f *.o *~ *.so *.bc *.ll

.PHONY: clean all