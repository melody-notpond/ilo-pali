#![feature(lang_items, prelude_import, panic_info_message, alloc_error_handler)]
#![no_std]

extern crate alloc;

use core::panic::PanicInfo;

#[prelude_import]
pub use {
    alloc::{
        borrow::ToOwned,
        boxed::Box,
        string::{String, ToString},
        vec,
        vec::Vec,
    },
    core::prelude::v1::*,
};

pub use no_std_compat::*;

#[lang = "eh_personality"]
#[no_mangle]
extern "C" fn eh_personality() {}

#[panic_handler]
fn panic_handler(info: &PanicInfo) -> ! {
    use fmt::Write;

    let mut writer = syscalls::UartWrite;
    let _ = write!(writer, "panic!");
    if let Some(message) = info.message() {
        let _ = write!(writer, " ");
        let _ = fmt::write(&mut writer, *message);
    }
    if let Some(location) = info.location() {
        let _ = write!(writer, " at {}", location);
    }
    let _ = writeln!(writer);
    let _ = syscalls::kill(syscalls::getpid());
    unreachable!();
}

use core::alloc::{GlobalAlloc, Layout};

#[repr(C)]
struct FreeBucket {
    next: *mut FreeBucket,
    _padding: usize,
}

struct Allocator {
    free_buckets: [*mut FreeBucket; 7],
}

impl Allocator {
    const fn new_const() -> Allocator {
        Allocator {
            free_buckets: [ptr::null_mut(); 7],
        }
    }
}

fn size_to_bucket(alloc: &mut Allocator, size: usize) -> Option<&mut *mut FreeBucket> {
    if size <= 16 {
        Some(&mut alloc.free_buckets[0])
    } else if size <= 64 {
        Some(&mut alloc.free_buckets[1])
    } else if size <= 256 {
        Some(&mut alloc.free_buckets[2])
    } else if size <= 1024 {
        Some(&mut alloc.free_buckets[3])
    } else if size <= 4096 {
        Some(&mut alloc.free_buckets[4])
    } else if size <= 16384 {
        Some(&mut alloc.free_buckets[5])
    } else if size <= 65536 {
        Some(&mut alloc.free_buckets[6])
    } else {
        None
    }
}

fn size_to_bucket_size(size: usize) -> usize {
    if size <= 16 {
        16
    } else if size <= 64 {
        64
    } else if size <= 256 {
        256
    } else if size <= 1024 {
        1024
    } else if size <= 4096 {
        4096
    } else if size <= 16384 {
        16384
    } else if size <= 65536 {
        65536
    } else {
        panic!("oh no");
    }
}

unsafe impl GlobalAlloc for sync::Mutex<Allocator> {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if layout.align() > 16 {
            return ptr::null_mut();
        }

        let mut lock = self.lock();

        if let Some(slot) = size_to_bucket(&mut *lock, layout.size()) {
            if slot.is_null() {
                let p: Option<*mut u8> = syscalls::alloc_page(
                    65,
                    &[
                        syscalls::PagePermissions::Read,
                        syscalls::PagePermissions::Write,
                    ],
                );
                if let Some(p) = p {
                    let size = size_to_bucket_size(layout.size());
                    let step = mem::size_of::<FreeBucket>() + size;
                    for i in (0..65 * syscalls::PAGE_SIZE).step_by(step) {
                        *(p.add(i) as *mut FreeBucket) = FreeBucket {
                            next: if i + step < 65 * syscalls::PAGE_SIZE {
                                p.add(i + step) as *mut FreeBucket
                            } else {
                                ptr::null_mut()
                            },
                            _padding: 0,
                        };
                    }

                    *slot = p as *mut FreeBucket;
                } else {
                    return ptr::null_mut();
                }
            }

            let bucket = *slot;

            let bucket = &mut *bucket;
            *slot = bucket.next;
            bucket.next = ptr::null_mut();

            (bucket as *mut FreeBucket).add(1) as *mut u8
        } else {
            let p = syscalls::alloc_page(
                (layout.size() + syscalls::PAGE_SIZE - 1) / syscalls::PAGE_SIZE,
                &[
                    syscalls::PagePermissions::Read,
                    syscalls::PagePermissions::Write,
                ],
            );

            if let Some(p) = p {
                p
            } else {
                ptr::null_mut()
            }
        }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        let bucket = (ptr as *mut FreeBucket).sub(1);
        let mut lock = self.lock();
        if let Some(slot) = size_to_bucket(&mut *lock, layout.size()) {
            (*bucket).next = *slot;
            *slot = bucket;
        } else {
            let _ = syscalls::dealloc_page(
                bucket,
                (layout.size() + syscalls::PAGE_SIZE - 1) / syscalls::PAGE_SIZE,
            );
        }
    }
}

#[global_allocator]
static GLOBAL_ALLOCATOR: sync::Mutex<Allocator> = sync::Mutex::new(Allocator::new_const());

#[alloc_error_handler]
fn alloc_error_handler(layout: Layout) -> ! {
    panic!("layout {:?} is invalid", layout);
}

pub mod sync;
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
