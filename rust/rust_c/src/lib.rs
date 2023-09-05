#![no_std]
#![feature(alloc_error_handler)]
#![feature(vec_into_raw_parts)]
#![feature(error_in_core)]
#![allow(unused_unsafe)]
extern crate alloc;

use alloc::boxed::Box;
use alloc::format;

use core::alloc::Layout;
use core::panic::PanicInfo;

use cstr_core::CString;
use cty::c_void;

// use crate::bindings::{LogRustPanic, PrintString};

// mod bindings;
mod my_alloc;
mod errors;
mod interfaces;

#[cfg(not(test))]
#[alloc_error_handler]
fn oom(layout: Layout) -> ! {
    // unsafe {
    //     LogRustPanic(
    //         CString::new(format!("Out of memory: {:?}", layout))
    //             .unwrap()
    //             .into_raw(),
    //     )
    // };
    loop {}
}

#[cfg(not(test))]
#[panic_handler]
fn panic(e: &PanicInfo) -> ! {
    // unsafe {
    //     LogRustPanic(
    //         CString::new(format!("rust panic: {:?}", e))
    //             .unwrap()
    //             .into_raw(),
    //     )
    // }
    loop {}
}

#[no_mangle]
pub extern "C" fn free_rust_value(any_ptr: *mut c_void) {
    if any_ptr.is_null() {
        return;
    }
    unsafe {
        drop(Box::from_raw(any_ptr));
    }
}
