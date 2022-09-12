TARGET = riscv64-unknown-elf
CC     = $(TARGET)-gcc
AR	   = $(TARGET)-ar
CFLAGS = -march=rv64gc -mabi=lp64d -static -mcmodel=medany -nostdlib -g -Wall -Wextra -I../../include/
ifeq ($(CC),clang)
	CFLAGS += -target $(TARGET) -mno-relax -Wno-unused-command-line-argument
	AR = llvm-ar
endif

CODE = src/

.PHONY: all clean

all: $(CODE)*.c $(CODE)*.s
	$(CC) $(CFLAGS) -c $?
	$(AR) rc libc.a *.o
	-rm *.o
	mv libc.a ../../build/lib/

clean:
	-rm *.o *.a
