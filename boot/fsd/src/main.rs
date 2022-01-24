#![no_std]

extern crate alloc;

use std::syscalls::UartWrite;
use std::fmt::Write;
use alloc::string::String;

fn main() {
    let s = String::from("hewo from rust!");
    let _ = writeln!(UartWrite, "msg: {}", s);
    drop(s);
}
