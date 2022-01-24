#![feature(lang_items, prelude_import, panic_info_message)]
#![no_std]

use core::panic::PanicInfo;

pub use no_std_compat::*;

#[prelude_import]
pub use core::prelude::v1::*;

#[lang = "eh_personality"]
#[no_mangle]
extern "C" fn eh_personality() {}

#[panic_handler]
fn panic_handler(info: &PanicInfo) -> ! {
    use fmt::Write;

    let mut writer = syscalls::UartWrite;
    let _ = writeln!(writer, "panic!");
    if let Some(message) = info.message() {
        let _ = write!(writer, " ");
        let _ = fmt::write(&mut writer, *message);
    }
    if let Some(location) = info.location() {
        let _ = writeln!(writer, " at {}", location);
    }
    let _ = syscalls::kill(syscalls::getpid());
    unreachable!();
}

pub mod syscalls;

#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct Capability(u128);

static mut ARGS: *const u8 = ptr::null();
static mut ARG_SIZE: usize = 0;
static mut CAPABILITY: Capability = Capability(0);

pub mod env {
    pub fn get_args<T>() -> Option<&'static T> {
        unsafe {
            if !super::ARGS.is_null() && super::mem::size_of::<T>() <= super::ARG_SIZE {
                Some(&*(super::ARGS as *const T))
            } else {
                None
            }
        }
    }

    pub fn get_capability() -> super::Capability {
        unsafe { super::CAPABILITY }
    }
}

mod entry {
    #[no_mangle]
    unsafe extern "C" fn _start(args: *const u8, size: usize, cap_high: u64, cap_low: u64) -> ! {
        extern "C" {
            fn main(argc: isize, argv: *const *const u8) -> isize;
        }

        super::ARGS = args;
        super::ARG_SIZE = size;
        super::CAPABILITY = super::Capability((cap_high as u128) << 64 | cap_low as u128);

        main(0, core::ptr::null());

        let _ = super::syscalls::kill(super::syscalls::getpid());
        unreachable!();
    }

    #[lang = "start"]
    fn lang_start<T>(main: fn() -> T, _: isize, _: *const *const u8) -> isize {
        main();
        0
    }
}
