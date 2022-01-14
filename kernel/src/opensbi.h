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

// sbi_set_timer(unsigned long long) -> struct sbiret
// Sets the timer value.
struct sbiret sbi_set_timer(unsigned long long stime_value);

// sbi_hart_start(unsigned long, unsigned long, unsigned long) -> struct sbiret
// Starts a hart at the given start address with the given opaque data.
struct sbiret sbi_hart_start(unsigned long hartid, unsigned long start_addr, unsigned long opaque);

// sbi_hart_stop(void) -> struct sbiret
// Stops a hart.
struct sbiret sbi_hart_stop(void);

// sbi_hart_get_status(unsigned long) -> struct sbiret
// Gets the current status of a hart.
struct sbiret sbi_hart_get_status(unsigned long hartid);

#endif /* OPENSBI_H */

