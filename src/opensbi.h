#ifndef OPENSBI_H
#define OPENSBI_H

enum {
	SBIRET_ERROR_CODE_SUCCESS               =  0,
	SBIRET_ERROR_CODE_FAILED                = -1,
	SBIRET_ERROR_CODE_UNSUPPORTED           = -2,
	SBIRET_ERROR_CODE_INVALID_PARAMETER     = -3,
	SBIRET_ERROR_CODE_DENIED                = -4,
	SBIRET_ERROR_CODE_INVALID_ADDRESS       = -5,
	SBIRET_ERROR_CODE_ALREADY_AVAILABLE     = -6
};

struct sbiret {
	unsigned long error;
	unsigned long value;
};

// sbi_console_putchar(char) -> void
// Puts a character onto the UART port.
void sbi_console_putchar(char);

// sbi_console_getchar() -> int
// Gets a character from the UART port.
int sbi_console_getchar();

#endif /* OPENSBI_H */

