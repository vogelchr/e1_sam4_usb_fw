GPS Displined Oscillator - Implementation for SAM4S on osmo e1 to usb interface

SAM4Sxy Devices
---------------

x: Memory
	D32 2 MB   Flash 160 kB SRAM
	D16 1 MB   Flash 160 kB SRAM
	A16 1 MB   Flash 160 kB SRAM
	16  1 MB   Flash 128 kB SRAM
	8   512 kB Flash 128 kB SRAM
	4   256 kB Flash 64  kB SRAM
	2   128 kB FLash 64  kB SRAM

y: Package
	A: 48 Pin
	B: 64 Pin
	C: 100 Pin


Pollin Eval Board
-----------------
	ATSAM4S4C, 256kB Flash, 64kB SRAM, 100 pin TQFP


Memory Map, Start of Regions (sam4s4c_flash.ld)
----------------------------

	Flash 0x0040_0000
	Flash 0x0050_0000 (D32)
	Flash 0x0048_0000 (D16, SA16)
	SRAM  0x2000 0000

/* Memory Spaces Definitions */
MEMORY
{
  rom (rx)  : ORIGIN = 0x00400000, LENGTH = 0x00040000
  ram (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00010000
}

/* The stack size used by the application. NOTE: you need to adjust according to your application. */
STACK_SIZE = DEFINED(STACK_SIZE) ? STACK_SIZE : 0x400;

/* The heapsize used by the application. NOTE: you need to adjust according to your application. */
HEAP_SIZE = DEFINED(HEAP_SIZE) ? HEAP_SIZE : 0x200;

Flash vector table:
-------------------

00000000: 4808 0020 4401 4000 4001 4000 4001 4000  H.. D.@.@.@.@.@.
          \---v---/ \---v---/ \---v---/ \---c---/
           Stackptr  Reset       NMI     HardFlt

Stack Ptr 0x2000848
Reset     0x0040144
