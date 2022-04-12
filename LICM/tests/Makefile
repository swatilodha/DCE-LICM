all: test1 test2 test3

test1:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c benchmark1.c -o benchmark1.bc
	opt -mem2reg benchmark1.bc -o benchmark1-m2r.bc

test2:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c benchmark2.c -o benchmark2.bc
	opt -mem2reg benchmark2.bc -o benchmark2-m2r.bc

test3:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c benchmark3.c -o benchmark3.bc
	opt -mem2reg benchmark3.bc -o benchmark3-m2r.bc

