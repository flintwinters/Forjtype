fj: Forj.c Vect.c
	gcc Forj.c -g -o fj
rv: 
	@riscv64-unknown-elf-as setup.s -g -o setup.o &&\
	riscv64-unknown-elf-gcc \
		-mcmodel=medany \
		-T linker.ld \
		-O0 -g \
		-c Forj.c \
		-o fjrv.o \
		-ffreestanding \
		-static -nostdlib -lgcc \
		-fverbose-asm && \
	riscv64-unknown-elf-gcc \
		-mcmodel=medany \
		-T linker.ld \
		setup.o \
		fjrv.o \
		-o fjrv \
		-ffreestanding \
		-O0 \
		-static \
		-nostdlib \
		-lgcc && \
	( \
		qemu-system-riscv64 \
			-s -S \
			-machine virt \
			-cpu rv64 \
			-nographic \
			-serial mon:stdio \
			-bios none \
			-kernel fjrv & \
			gdb-multiarch \
				-l 2 -q fjrv \
				-x `pwd`/pyg.py \
				-ex "py connect()" \
	)
	pkill -f qemu-system-riscv64
val: fj
	valgrind ./fj
gdb: fj
	gdb ./fj
