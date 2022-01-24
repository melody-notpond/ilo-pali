#![no_std]

use std::syscalls::UartWrite;
use std::fmt::Write;

fn main() {
    let s = "hewo from rust!\n";
    let _ = writeln!(UartWrite, "msg: {}", s);
    panic!("uwu");
}
