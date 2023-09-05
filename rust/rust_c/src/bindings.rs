use cty::{c_char, c_void};

#[cfg(not(test))]
extern "C" {
    pub fn PrintArray(name: *const c_char, data: *const u8, length: u16);
    pub fn PrintString(name: *mut c_char);
    pub fn LogRustMalloc(p: *mut c_void, size: u32);
    pub fn LogRustFree(p: *mut c_void);
    pub fn LogRustPanic(p: *mut c_char);
}

#[cfg(test)]
pub fn PrintArray(name: *const c_char, data: *const u8, length: u16) {}

#[cfg(test)]
pub fn PrintString(name: *mut c_char) {}

#[cfg(test)]
pub fn LogRustMalloc(p: *mut c_void, size: u32) {}

#[cfg(test)]
pub fn LogRustPanic(p: *mut c_void, size: u32) {}

#[cfg(test)]
pub fn LogRustFree(p: *mut c_void) {}
