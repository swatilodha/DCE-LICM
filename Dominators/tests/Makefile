all: test1

test1:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c benchmark1.c -o benchmark1.bc
	opt -mem2reg benchmark1.bc -o benchmark1-m2r.bc

