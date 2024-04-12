TARGET = riscv64-unknown-elf
CC     = clang
CFLAGS = -march=rv64gc -mabi=lp64d -static -mcmodel=medany -fvisibility=hidden -nostdlib -g -Wall -Wextra -L../../build/lib/ -I../../include/
ifeq ($(CC),clang)
	CFLAGS += -target $(TARGET) -mno-relax -Wno-unused-command-line-argument
endif

LIBS = -lc #-lfdt -lfat -lsync -ljoin -lsyscall -liter -lalloc -lformat -lcore
CODE = src/

.PHONY: all

all: $(CODE)*.c
	$(CC) $? $(CFLAGS) $(LIBS) -o ../../build/boot/uwu
