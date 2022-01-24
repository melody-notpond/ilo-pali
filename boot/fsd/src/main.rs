#![no_std]

extern crate alloc;

use std::syscalls::UartWrite;
use std::fmt::Write;
use std::env::get_capability;
use std::syscalls::{self, Time, MessageType};

fn main() {
    let _ = writeln!(UartWrite, "spawned fsd!");
    if let Some(cap) = get_capability() {
        syscalls::send(true, cap, MessageType::Signal, 0, 0).unwrap();
        let _ = writeln!(UartWrite, "uwu");

        loop {
            syscalls::sleep(Time {
                seconds: 0,
                micros: 10,
            });
        }
    }
}
