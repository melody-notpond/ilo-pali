TARGET = riscv64-unknown-elf
CC     = clang
CFLAGS = -march=rv64gc -mabi=lp64d -static -mcmodel=medany -fvisibility=hidden -nostdlib -nostartfiles -g -Wall -Wextra
ifeq ($(CC),clang)
	CFLAGS += -target $(TARGET) -mno-relax -Wno-unused-command-line-argument -Wthread-safety
endif

CODE = src/

.PHONY: all

all: $(CODE)*.s $(CODE)*.c $(CODE)*/*.c
	$(CC) $(CFLAGS) -Tkernel.ld $? -o ../build/kernel
