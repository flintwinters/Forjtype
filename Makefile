all:
	@python3 challenger.py
fj: Forj.c Vect.c
	@gcc Forj.c -g -o fj
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
	@valgrind --errors-for-leak-kinds=all --error-exitcode=1 --leak-check=full --show-leak-kinds=all ./fj 2> val.log || \
	if [ $$? -ne 0 ]; then \
		echo "\033[31;1mValgrind: Errors or leaks found. Check val.log\033[0m"; \
	fi
gdb: fj
	gdb --args ./fj challenge
