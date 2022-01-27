use crate::Capability;

pub static PAGE_SIZE: usize = 4096;

#[allow(clippy::too_many_arguments)]
#[inline]
unsafe fn syscall(
    syscall: u64,
    a1: u64,
    a2: u64,
    a3: u64,
    a4: u64,
    a5: u64,
    a6: u64,
    a7: u64,
) -> (u64, u64) {
    use core::arch::asm;

    let res1: u64;
    let res2: u64;
    asm!(
        "ecall",
        in("a0") syscall,
        in("a1") a1,
        in("a2") a2,
        in("a3") a3,
        in("a4") a4,
        in("a5") a5,
        in("a6") a6,
        in("a7") a7,
        lateout("a0") res1,
        lateout("a1") res2,
    );

    (res1, res2)
}

pub struct UartWrite;

impl super::fmt::Write for UartWrite {
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        uart_write(s.as_ptr(), s.len());
        Ok(())
    }
}

pub fn uart_write(data: *const u8, len: usize) {
    unsafe {
        syscall(0, data as u64, len as u64, 0, 0, 0, 0, 0);
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum PagePermissions {
    Read,
    Write,
    Execute,
}

fn array_to_raw_perms(perms: &[PagePermissions]) -> u64 {
    use PagePermissions::*;
    let mut p = 0;
    if perms.contains(&Read) {
        p |= 0b100;
    }

    if perms.contains(&Write) {
        p |= 0b010;
    }

    if perms.contains(&Execute) {
        p |= 0b001;
    }

    p
}

pub fn alloc_page<T>(count: usize, perms: &[PagePermissions]) -> Option<*mut T> {
    unsafe {
        let p = syscall(1, count as u64, array_to_raw_perms(perms), 0, 0, 0, 0, 0).0 as *mut T;
        if p.is_null() {
            None
        } else {
            Some(p)
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub enum PagePermissionsError {
    NeverAllocated,
    InvalidPermissions,
}

pub fn page_permissions<T>(
    addr: *mut T,
    count: usize,
    perms: &[PagePermissions],
) -> Result<(), PagePermissionsError> {
    match unsafe {
        syscall(
            2,
            addr as u64,
            count as u64,
            array_to_raw_perms(perms),
            0,
            0,
            0,
            0,
        )
        .0
    } {
        0 => Ok(()),
        1 => Err(PagePermissionsError::NeverAllocated),
        2 => Err(PagePermissionsError::InvalidPermissions),

        _ => unreachable!(),
    }
}

#[derive(Debug, Clone, Copy)]
pub enum DeallocPageError {
    NeverAllocated,
}

pub fn dealloc_page<T>(addr: *mut T, count: usize) -> Result<(), DeallocPageError> {
    if unsafe { syscall(3, addr as u64, count as u64, 0, 0, 0, 0, 0).0 } == 0 {
        Ok(())
    } else {
        Err(DeallocPageError::NeverAllocated)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Pid(u64);

pub fn getpid() -> Pid {
    Pid(unsafe { syscall(4, 0, 0, 0, 0, 0, 0, 0).0 })
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Uid(u64);

#[derive(Debug, Clone, Copy)]
pub enum GetUidError {
    ProcessDoesNotExist,
}

pub fn getuid(pid: Pid) -> Result<Uid, GetUidError> {
    match unsafe { syscall(5, pid.0, 0, 0, 0, 0, 0, 0).0 } {
        u64::MAX => Err(GetUidError::ProcessDoesNotExist),
        v => Ok(Uid(v)),
    }
}

#[derive(Debug, Clone, Copy)]
pub enum SetUidError {
    ProcessDoesNotExist,
    InsufficientPermissions,
}

pub fn setuid(pid: Pid, uid: Uid) -> Result<(), SetUidError> {
    match unsafe { syscall(6, pid.0, uid.0, 0, 0, 0, 0, 0).0 } {
        0 => Ok(()),
        1 => Err(SetUidError::ProcessDoesNotExist),
        2 => Err(SetUidError::InsufficientPermissions),
        _ => unreachable!(),
    }
}

#[derive(Debug, Clone, Copy)]
pub struct Time {
    pub seconds: u64,
    pub micros: u64,
}

pub fn sleep(interval: Time) -> Time {
    let (seconds, micros) = unsafe { syscall(7, interval.seconds, interval.micros, 0, 0, 0, 0, 0) };

    Time { seconds, micros }
}

pub fn spawn_process<T>(
    exec: *const u8,
    exec_size: usize,
    args: *const T,
    arg_size: usize,
    capability: Option<&mut Capability>,
) -> Option<Pid> {
    match unsafe {
        syscall(
            8,
            exec as u64,
            exec_size as u64,
            args as u64,
            arg_size as u64,
            capability.map(|v| v as *mut Capability as u64).unwrap_or(0),
            0,
            0,
        )
        .0
    } {
        u64::MAX => None,
        v => Some(Pid(v)),
    }
}

#[derive(Debug, Clone, Copy)]
pub enum KillError {
    ProcessDoesNotExist,
    InsufficientPermissions,
}

pub fn kill(pid: Pid) -> Result<(), KillError> {
    match unsafe { syscall(9, pid.0, 0, 0, 0, 0, 0, 0).0 } {
        0 => Ok(()),
        1 => Err(KillError::ProcessDoesNotExist),
        2 => Err(KillError::InsufficientPermissions),
        _ => unreachable!(),
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum MessageType {
    Signal,
    Integer,
    Pointer,
    Data,
    Interrupt,
}

#[derive(Debug, Clone, Copy)]
pub enum SendError {
    InvalidArguments,
    MessageQueueFull,
}

pub fn send(
    block: bool,
    channel: Capability,
    type_: MessageType,
    data: u64,
    metadata: u64,
) -> Result<(), SendError> {
    let type_ = match type_ {
        MessageType::Signal => 0,
        MessageType::Integer => 1,
        MessageType::Pointer => 2,
        MessageType::Data => 3,
        MessageType::Interrupt => return Err(SendError::InvalidArguments),
    };

    match unsafe {
        syscall(
            10,
            block as u64,
            &channel as *const Capability as u64,
            type_,
            data,
            metadata,
            0,
            0,
        )
        .0
    } {
        0 => Ok(()),
        1 => Err(SendError::InvalidArguments),
        2 => Err(SendError::MessageQueueFull),
        _ => unreachable!(),
    }
}

#[derive(Debug, Clone)]
pub struct Message {
    pub pid: Pid,
    pub type_: MessageType,
    pub data: u64,
    pub metadata: u64,
}

impl Drop for Message {
    fn drop(&mut self) {
        match self.type_ {
            MessageType::Signal | MessageType::Integer | MessageType::Interrupt => (),

            MessageType::Data => {
                let _ = dealloc_page(self.data as *mut u8, 1);
            }

            MessageType::Pointer => {
                let _ = dealloc_page(self.data as *mut u8, (self.metadata as usize + PAGE_SIZE - 1) / PAGE_SIZE);
            }
        }
    }
}

pub fn recv(block: bool, channel: Capability) -> Option<Message> {
    let (mut pid, mut type_, mut data, mut metadata) = (0, 0u64, 0, 0);

    if unsafe {
        syscall(
            11,
            block as u64,
            &channel as *const Capability as u64,
            &mut pid as *mut u64 as u64,
            &mut type_ as *mut u64 as u64,
            &mut data as *mut u64 as u64,
            &mut metadata as *mut u64 as u64,
            0,
        )
        .0
    } != 0
    {
        return None;
    }

    Some(Message {
        pid: Pid(pid),
        type_: match type_ {
            0 => MessageType::Signal,
            1 => MessageType::Integer,
            2 => MessageType::Pointer,
            3 => MessageType::Data,
            4 => MessageType::Interrupt,
            _ => unreachable!(),
        },
        data,
        metadata,
    })
}

pub enum LockableSize {
    U8,
    U16,
    U32,
    U64,
}

pub trait Lockable: Sized {
    fn size() -> LockableSize;

    fn into(self) -> u64;
}

impl Lockable for u8 {
    fn size() -> LockableSize {
        LockableSize::U8
    }

    fn into(self) -> u64 {
        self as u64
    }
}

impl Lockable for u16 {
    fn size() -> LockableSize {
        LockableSize::U16
    }

    fn into(self) -> u64 {
        self as u64
    }
}

impl Lockable for u32 {
    fn size() -> LockableSize {
        LockableSize::U32
    }

    fn into(self) -> u64 {
        self as u64
    }
}

impl Lockable for u64 {
    fn size() -> LockableSize {
        LockableSize::U64
    }

    fn into(self) -> u64 {
        self as u64
    }
}

impl Lockable for i8 {
    fn size() -> LockableSize {
        LockableSize::U8
    }

    fn into(self) -> u64 {
        self as u64
    }
}

impl Lockable for i16 {
    fn size() -> LockableSize {
        LockableSize::U16
    }

    fn into(self) -> u64 {
        self as u64
    }
}

impl Lockable for i32 {
    fn size() -> LockableSize {
        LockableSize::U32
    }

    fn into(self) -> u64 {
        self as u64
    }
}

impl Lockable for i64 {
    fn size() -> LockableSize {
        LockableSize::U64
    }

    fn into(self) -> u64 {
        self as u64
    }
}

use core::sync::atomic::{
    AtomicI16, AtomicI32, AtomicI64, AtomicI8, AtomicU16, AtomicU32, AtomicU64, AtomicU8, Ordering,
};

impl Lockable for AtomicU8 {
    fn size() -> LockableSize {
        LockableSize::U8
    }

    fn into(self) -> u64 {
        self.load(Ordering::Acquire) as u64
    }
}

impl Lockable for AtomicU16 {
    fn size() -> LockableSize {
        LockableSize::U16
    }

    fn into(self) -> u64 {
        self.load(Ordering::Acquire) as u64
    }
}

impl Lockable for AtomicU32 {
    fn size() -> LockableSize {
        LockableSize::U32
    }

    fn into(self) -> u64 {
        self.load(Ordering::Acquire) as u64
    }
}

impl Lockable for AtomicU64 {
    fn size() -> LockableSize {
        LockableSize::U64
    }

    fn into(self) -> u64 {
        self.load(Ordering::Acquire) as u64
    }
}

impl Lockable for AtomicI8 {
    fn size() -> LockableSize {
        LockableSize::U8
    }

    fn into(self) -> u64 {
        self.load(Ordering::Acquire) as u64
    }
}

impl Lockable for AtomicI16 {
    fn size() -> LockableSize {
        LockableSize::U16
    }

    fn into(self) -> u64 {
        self.load(Ordering::Acquire) as u64
    }
}

impl Lockable for AtomicI32 {
    fn size() -> LockableSize {
        LockableSize::U32
    }

    fn into(self) -> u64 {
        self.load(Ordering::Acquire) as u64
    }
}

impl Lockable for AtomicI64 {
    fn size() -> LockableSize {
        LockableSize::U64
    }

    fn into(self) -> u64 {
        self.load(Ordering::Acquire) as u64
    }
}

#[derive(Debug, Copy, Clone)]
pub enum LockType {
    Wait,
    Wake,
}

pub fn lock<T>(ref_: *const T, type_: LockType, value: T)
where
    T: Lockable,
{
    unsafe {
        syscall(
            12,
            ref_ as u64,
            match T::size() {
                LockableSize::U8 => 0,
                LockableSize::U16 => 2,
                LockableSize::U32 => 4,
                LockableSize::U64 => 6,
            } | match type_ {
                LockType::Wait => 0,
                LockType::Wake => 1,
            },
            value.into(),
            0,
            0,
            0,
            0,
        );
    }
}

unsafe extern "C" fn thread_unpack_args<F, T>(args: *const u8, _size: usize, cap_high: u64, cap_low: u64) -> !
    where F: FnOnce(Option<Capability>) -> T,
          F: Send + 'static
{
    let cap = if cap_high != 0 || cap_low != 0 {
        Some(Capability::from(cap_low as u128 | (cap_high as u128) << 64))
    } else {
        None
    };

    use super::boxed::Box;
    let func = Box::from_raw(args as *mut F);

    func.call_once((cap,));

    let _ = kill(getpid());
    unreachable!();
}

pub fn spawn_thread<F, T>(
    func: F,
    capability: Option<&mut Capability>,
) -> Option<Pid>
    where F: FnOnce(Option<Capability>) -> T,
          F: Send + 'static
{
    let size = super::mem::size_of::<F>();

    use super::Box;
    let func = Box::new(func);
    let func = Box::into_raw(func);

    match unsafe {
        syscall(
            13,
            thread_unpack_args::<F, T> as *const u8 as u64,
            func as u64,
            size as u64,
            capability.map(|v| v as *mut Capability as u64).unwrap_or(0),
            0,
            0,
            0,
        )
        .0
    } {
        u64::MAX => {
            unsafe {
                Box::from_raw(func);
            }
            None
        }
        v => Some(Pid(v)),
    }
}

pub fn subscribe_to_interrupt(id: u32, capability: Capability) {
    unsafe {
        syscall(
            14,
            id as u64,
            &capability as *const Capability as u64,
            0,
            0,
            0,
            0,
            0,
        );
    }
}

pub fn alloc_pages_physical<T>(
    addr: *const T,
    count: usize,
    perms: &[PagePermissions],
    capability: Capability,
) -> (*mut T, u64) {
    let (virtual_, physical) = unsafe {
        syscall(
            15,
            addr as u64,
            count as u64,
            array_to_raw_perms(perms),
            &capability as *const Capability as u64,
            0,
            0,
            0,
        )
    };

    (virtual_ as *mut T, physical)
}

pub enum TransferCapabilityError {
    ProcessDoesNotExist
}

pub fn transfer_capability(cap: Capability, pid: Pid) -> Result<(), TransferCapabilityError> {
    match unsafe {
        syscall(16, &cap as *const Capability as u64, pid.0, 0, 0, 0, 0, 0).0
    } {
        0 => Ok(()),
        1 => Err(TransferCapabilityError::ProcessDoesNotExist),
        _ => unreachable!(),
    }
}

pub fn clone_capability(original: Capability, new: &mut Capability) {
    unsafe {
        syscall(17, &original as *const Capability as u64, new as *mut Capability as u64, 0, 0, 0, 0, 0);
    }
}

pub fn create_capability() -> (Capability, Capability) {
    let mut cap1 = Capability(0);
    let mut cap2 = Capability(0);

    unsafe {
        syscall(18, &mut cap1 as *mut Capability as u64, &mut cap2 as *mut Capability as u64, 0, 0, 0, 0, 0);
    }

    (cap1, cap2)
}

