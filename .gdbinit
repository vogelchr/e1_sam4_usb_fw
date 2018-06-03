define init_regs
	monitor reg r0  0xdeadbeef
	monitor reg r1  0xdeadbeef
	monitor reg r2  0xdeadbeef
	monitor reg r3  0xdeadbeef
	monitor reg r4  0xdeadbeef
	monitor reg r5  0xdeadbeef
	monitor reg r6  0xdeadbeef
	monitor reg r7  0xdeadbeef
	monitor reg r8  0xdeadbeef
	monitor reg r9  0xdeadbeef
	monitor reg r10 0xdeadbeef
	monitor reg r11 0xdeadbeef
	monitor reg r12 0xdeadbeef
end

define do_load
	monitor reset halt
	init_regs
	load
	# load will set pc to the entry symbol (gcc -Wl,-eReset_Handler),
	# but will *not* set the stack pointer. Let's just set them all explicitly.
	eval "monitor reg sp %p", &_estack
	eval "monitor reg lr %p", &Reset_Handler
	eval "monitor reg pc %p", &Reset_Handler
end

target remote localhost:3333
file "sam4s_fw.elf"

do_load
