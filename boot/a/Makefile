CFLAGS = -march=rv64gc -mabi=lp64d -static -mcmodel=medany -fvisibility=hidden -nostdlib -g -Wall -Wextra
ifeq ($(CC),clang)
	CFLAGS += -target $(TARGET) -mno-relax -Wno-unused-command-line-argument
endif

CODE = src/

.PHONY: all

all: $(CODE)*.s $(CODE)*.c
	$(CC) $(CFLAGS) $? -o ../../build/boot/a
