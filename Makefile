fj: Forj.c Vect.c
	gcc Forj.c -g -o fj
val: fj
	valgrind ./fj
gdb: fj
	gdb ./fj
