all: test1 test2 test3 test4 test5

test1:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c mbenchmark1.c -o mbenchmark1.bc
	opt -mem2reg mbenchmark1.bc -o mbenchmark1-m2r.bc
	llvm-dis mbenchmark1-m2r.bc

test2:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c mbenchmark2.c -o mbenchmark2.bc
	opt -mem2reg mbenchmark2.bc -o mbenchmark2-m2r.bc
	llvm-dis mbenchmark2-m2r.bc

test3:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c mbenchmark3.c -o mbenchmark3.bc
	opt -mem2reg mbenchmark3.bc -o mbenchmark3-m2r.bc
	llvm-dis mbenchmark3-m2r.bc

test4:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c mbenchmark4.c -o mbenchmark4.bc
	opt -mem2reg mbenchmark4.bc -o mbenchmark4-m2r.bc
	llvm-dis mbenchmark4-m2r.bc

test5:
	clang -Xclang -disable-O0-optnone -O0 -emit-llvm -c mbenchmark5.c -o mbenchmark5.bc
	opt -mem2reg mbenchmark5.bc -o mbenchmark5-m2r.bc
	llvm-dis mbenchmark5-m2r.bc