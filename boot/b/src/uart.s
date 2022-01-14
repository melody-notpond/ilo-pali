.global uart_write

uart_write:
    mv a2, a1
    mv a1, a0
    li a0, 0
    ecall
    ret
