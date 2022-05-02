all: test1

test1:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c mbenchmark1.c -o mbenchmark1.bc
	opt -mem2reg mbenchmark1.bc -o mbenchmark1-m2r.bc
	llvm-dis mbenchmark1-m2r.bc