pub use core::sync::*;

use core::{
    cell::UnsafeCell,
    ops::{Deref, DerefMut},
    sync::atomic::Ordering,
};

pub struct Mutex<T> {
    lock: atomic::AtomicU8,
    data: UnsafeCell<T>,
}

unsafe impl<T> Sync for Mutex<T> {}

impl<T> Mutex<T> {
    pub const fn new(data: T) -> Mutex<T> {
        Mutex {
            lock: atomic::AtomicU8::new(0),
            data: UnsafeCell::new(data),
        }
    }

    pub fn lock(&self) -> MutexGuard<'_, T> {
        'outer: loop {
            for _ in 0..64 {
                match self
                    .lock
                    .compare_exchange_weak(0, 1, Ordering::Acquire, Ordering::Relaxed)
                {
                    Ok(_) => break 'outer MutexGuard { data: self },

                    Err(_) => core::hint::spin_loop(),
                }
            }

            use super::syscalls::{lock, LockType};
            lock(&self.lock, LockType::Wake, atomic::AtomicU8::new(0));
        }
    }
}

pub struct MutexGuard<'a, T> {
    data: &'a Mutex<T>,
}

impl<'a, T> Deref for MutexGuard<'a, T> {
    type Target = T;

    fn deref(&self) -> &T {
        unsafe { &*self.data.data.get() }
    }
}

impl<'a, T> DerefMut for MutexGuard<'a, T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { &mut *self.data.data.get() }
    }
}

impl<'a, T> Drop for MutexGuard<'a, T> {
    fn drop(&mut self) {
        self.data.lock.store(0, Ordering::Release);
    }
}
