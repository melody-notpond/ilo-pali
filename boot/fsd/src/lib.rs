#![no_std]

pub const GPT_SIGNATURE: u64 = 0x5452415020494645;

#[repr(C)]
#[derive(Debug, Clone)]
pub struct GptHeader {
    pub signature: u64,
    pub revision: u32,
    pub header_size: u32,
    pub header_crc32: u32,
    reserved: u32,
    pub current_lba: u64,
    pub backup_lba: u64,
    pub first_usable_lba: u64,
    pub last_usable_lba: u64,
    pub disk_guid: [u64; 2],
    pub entries_lba: u64,
    pub entry_count: u32,
    pub entry_size: u32,
    pub entry_crc32: u32
}

use std::Capability;
use std::syscalls::{self, Message, MessageType};

pub struct Sector(*const u8);

impl Sector {
    pub fn get(&self) -> *const u8 {
        self.0
    }
}

impl Drop for Sector {
    fn drop(&mut self) {
        let _ = syscalls::dealloc_page(self.0 as *mut u8, 1);
    }
}

pub fn read(block: Capability, sector: u64) -> Option<Sector> {
    if syscalls::send(true, block, MessageType::Signal, 0, sector).is_err() {
        return None;
    }

    match syscalls::recv(true, block) {
        Some(Message {
            type_: MessageType::Signal,
            data: 0,
            metadata: 0,
            ..
        }) => {
            match syscalls::recv(true, block) {
                Some(msg) if matches!(msg.type_, MessageType::Data) => {
                    Some(Sector(Message::into_raw(msg).0 as *const u8))
                }

                _ => None,
            }
        }

        _ => None,
    }
}

pub enum WriteError {
    SendError(syscalls::SendError),
    WriteError,
}

impl From<syscalls::SendError> for WriteError {
    fn from(e: syscalls::SendError) -> Self {
        WriteError::SendError(e)
    }
}

pub fn write(block: Capability, sector: u64, data: &[u8]) -> Result<(), WriteError> {
    syscalls::send(true, block, MessageType::Signal, 1, sector)?;
    syscalls::send(true, block, MessageType::Data, data.as_ptr() as u64, data.len() as u64)?;
    match syscalls::recv(true, block) {
        Some(Message { type_: MessageType::Signal, data: 0, metadata: 1, .. }) => Ok(()),
        _ => Err(WriteError::WriteError),
    }
}
