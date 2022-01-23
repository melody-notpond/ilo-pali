TARGET = riscv64-unknown-elf
CC     = $(TARGET)-gcc
AR	   = $(TARGET)-ar
CFLAGS = -march=rv64gc -mabi=lp64d -static -mcmodel=medany -nostdlib -g -Wall -Wextra -I../../include/
ifeq ($(CC),clang)
	CFLAGS += -target $(TARGET) -mno-relax -Wno-unused-command-line-argument
endif

CODE = src/

.PHONY: all clean

all: $(CODE)*.c
	$(CC) $(CFLAGS) -c $?
	$(AR) rc libphalloc.a *.o
	-rm *.o
	mv libphalloc.a ../../build/lib/

clean:
	-rm *.o *.a
