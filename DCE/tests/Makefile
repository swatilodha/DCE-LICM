all: test1 test2

test1:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c dce_test1.c -o dce_test1.bc
	opt -mem2reg dce_test1.bc -o dce_test1-m2r.bc

test2:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c dce_test2.c -o dce_test2.bc
	opt -mem2reg dce_test2.bc -o dce_test2-m2r.bc