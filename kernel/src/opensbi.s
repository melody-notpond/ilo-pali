.section .text

.global sbi_console_putchar
.global sbi_console_getchar

.global sbi_set_timer

.global sbi_hart_start
.global sbi_hart_stop
.global sbi_hart_get_status
.global sbi_hart_suspend

# EIDs are stored in a7
# FIDs are stored in a6

# sbi_console_putchar(char) -> void
# Puts a character onto the UART port.
sbi_console_putchar:
	li a6, 0
	li a7, 1
	ecall
	ret

# sbi_console_getchar() -> int
# Gets a character from the UART port.
sbi_console_getchar:
	li a6, 0
	li a7, 2
	ecall
	ret

# sbi_set_timer(unsigned long long) -> struct sbiret
# Sets the timer value.
sbi_set_timer:
    li a6, 0
    li a7, 0x54494D45
    ecall
    ret

# sbi_hart_start(unsigned long, unsigned long, unsigned long) -> struct sbiret
# Starts a hart at the given start address with the given opaque data.
sbi_hart_start:
    li a6, 0
    li a7, 0x48534d
    ecall
    ret

# sbi_hart_stop(void) -> struct sbiret
# Stops a hart.
sbi_hart_stop:
    li a6, 1
    li a7, 0x48534d
    ecall
    ret

# sbi_hart_get_status(unsigned long) -> struct sbiret
# Gets the current status of a hart.
sbi_hart_get_status:
    li a6, 2
    li a7, 0x48534d
    ecall
    ret

# sbi_hart_suspend(uint32_t, unsigned long, unsigned long) -> struct sbiret
# Suspends the current hart.
sbi_hart_suspend:
    li a6, 3
    li a7, 0x48534d
    ecall
    ret
