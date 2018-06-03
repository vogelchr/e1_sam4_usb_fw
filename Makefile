CROSS=arm-none-eabi-

OBJCOPY=$(CROSS)objcopy
CC=$(CROSS)gcc
LD=$(CROSS)ld

CHIP_LD=sam4s8b
CHIP_CPP=__SAM4S8B__

LDSCRIPT_PATH=Atmel.SAM4S_DFP.1.0.56/sam4s/gcc/gcc
LDSCRIPT=$(CHIP_LD)_sram.ld
VPATH=Atmel.SAM4S_DFP.1.0.56/sam4s/gcc/gcc/:\
	Atmel.SAM4S_DFP.1.0.56/sam4s/gcc/

CPU=-mthumb -mcpu=cortex-m4
CFLAGS=-Wall -Wextra -Wpedantic -Wno-unused -Wduplicated-cond \
	-Wduplicated-branches -Wlogical-op -Wrestrict -Wnull-dereference \
	-Wjump-misses-init -Wshadow -Wformat=2 \
	-Os -ggdb -g3 $(CPU)
CPPFLAGS=-D$(CHIP_CPP)=1 -DSAM4S=1 -DF_MCK_HZ=110592000 \
	-IAtmel.SAM4S_DFP.1.0.56/sam4s/include/ \
	-ICMSIS_5/CMSIS/Core/Include

OBJECTS=startup_sam4s.o newlib_syscalls.o sam4s_fw_main.o gps_steer.o \
	sam4s_clock.o sam4s_uart0_console.o sam4s_pinmux.o sam4s_dac.o sam4s_timer.o \
	sam4s_ssc.o sam4s_spi.o ssc_realign.o

all : sam4s_fw.elf

.PRECIOUS : %.elf %.map

# ISO C forbids conversion of function pointer to object pointer type
# [-Wpedantic] // .pfnXXXX_Handler = (void*) XXX_Handler,
startup_sam4s.o : CFLAGS += -Wno-pedantic

###
# will be helpful for flashing the chip later :-)
###
# %.hex : %.elf
# 	$(OBJCOPY) -O ihex $^ $@
# 
# %.bin : %.elf
# 	$(OBJCOPY) -O binary $^ $@

%.elf :$(OBJECTS)
	$(CC) -Wl,--defsym=HEAP_SIZE=0x1000 -Wl,--defsym=STACK_SIZE=0x1000 \
	-L$(LDSCRIPT_PATH) -T$(LDSCRIPT) -Wl,--gc-sections \
	-Wl,-Map=$*.map -Wl,-eReset_Handler $(CPU) -o $@ $^

%.o : $.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^

ifneq ($(MAKECMDGOALS),clean)
%.d : %.c
	$(CC) $(CPPFLAGS) -MM -o $@ $^

include $(OBJECTS:.o=.d)
endif

.PHONY : clean
clean :
	rm -f *.d *.o *.bin *.elf *.hex *.map *.bak *~
