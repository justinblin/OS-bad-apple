# Save original PATH and LD_LIBRARY_PATH
ORIGINAL_PATH := $(PATH)
ORIGINAL_LD_LIBRARY_PATH := $(LD_LIBRARY_PATH)

PATH := /usr/bin

# Set GCC path explicitly
GCC := /usr/bin/gcc

UTILS = init shell

CFLAGS = -std=c99 -m32 -nostdlib -g -O2 -Wall -Werror

all : $(UTILS)
	# Restore original environment after build
	export PATH=$(ORIGINAL_PATH)
	export LD_LIBRARY_PATH=$(ORIGINAL_LD_LIBRARY_PATH)

OFILES = sys.o crt0.o libc.o heap.o machine.o printf.o

# keep all files
.SECONDARY :

%.o : Makefile %.c
	$(GCC) -c -MD $(CFLAGS) $*.c

%.o : Makefile %.S
	$(GCC) -MD -m32 -c $*.S

%.o : Makefile %.s
	$(GCC) -MD -m32 -c $*.s

$(UTILS) : % : Makefile %.o $(OFILES)
	ld -N -m elf_i386 -e start -Ttext=0x80000000 -o $@ $*.o $(OFILES)

clean ::
	rm -f *.o
	rm -f *.d
	rm -f $(UTILS)

-include *.d

.PHONY: all clean