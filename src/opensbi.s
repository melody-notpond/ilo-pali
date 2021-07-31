.section .text

.global sbi_console_putchar
.global sbi_console_getchar

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

