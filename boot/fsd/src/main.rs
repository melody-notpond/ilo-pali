#![no_std]

extern crate alloc;

use std::Capability;
use std::syscalls::UartWrite;
use std::fmt::Write;
use std::env::get_capability;
use std::syscalls::{self, MessageType};
use hashbrown::HashSet;

fn main() {
    let mut states = HashSet::new();
    let _ = writeln!(UartWrite, "spawned fsd!");
    if let Some(cap) = get_capability() {
        syscalls::send(true, cap, MessageType::Signal, 0, 1).unwrap();

        while let Some(message) = syscalls::recv(true, cap) {
            if states.contains(&message.pid) {
                if let MessageType::Integer = message.type_ {
                    let block = Capability::from((message.metadata as u128) << 64 | message.data as u128);
                    loop {
                        if let Some(pid) = syscalls::spawn_thread(move |_| {
                            let _ = syscalls::send(true, block, MessageType::Signal, 0, 0);
                            while let Some(message) = syscalls::recv(true, block) {
                                let _ = writeln!(UartWrite, "{:?}", message);
                            }
                        }, None) {
                            if syscalls::transfer_capability(block, pid).is_ok() {
                                break;
                            }
                        }
                    }

                    states.remove(&message.pid);
                } else {
                    let _ = writeln!(UartWrite, "invalid capability passed in");
                }
            } else if let MessageType::Signal = message.type_ {
                match message.data {
                    0 => {
                        if message.data != 0 {
                            let _ = writeln!(UartWrite, "unable to acquire filesystem driver status");
                            let _ = syscalls::kill(syscalls::getpid());
                            unreachable!();
                        } else {
                            let _ = writeln!(UartWrite, "acquired filesystem driver status!");
                        }
                    }

                    1 => {
                        states.insert(message.pid);
                    }
 
                    _ => {
                        let _ = writeln!(UartWrite, "unknown signal {}", message.data);
                        continue;
                    }
                }
            }
        }
    }
}
