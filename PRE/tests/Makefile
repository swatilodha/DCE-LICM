all: test1 test2

test1:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c mbenchmark1.c -o mbenchmark1.bc
	opt -mem2reg mbenchmark1.bc -o mbenchmark1-m2r.bc
	llvm-dis mbenchmark1-m2r.bc

test2:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c mbenchmark2.c -o mbenchmark2.bc
	opt -mem2reg mbenchmark2.bc -o mbenchmark2-m2r.bc
	llvm-dis mbenchmark2-m2r.bc