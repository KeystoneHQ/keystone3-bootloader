pub mod signature;
use crate::interfaces::signature::verify_signature;
use alloc::string::{String, ToString};
use core::slice;
use cstr_core::CStr;
use cty::c_char;

fn recover_c_char(s: *mut c_char) -> Option<String> {
    if s.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(s).to_str().ok().map(|v| v.to_string()) }
}

#[no_mangle]
pub extern "C" fn verify_frimware_signature(
    signature_ptr: *mut c_char,
    message_hash_ptr: *mut u8,
    pubkey_ptr: *mut u8
) -> bool {
    if message_hash_ptr.is_null() || pubkey_ptr.is_null() {
        return false;
    }
    let signature = match recover_c_char(signature_ptr) {
        Some(v) => v,
        None => return false,
    };
    let message_hash = unsafe { slice::from_raw_parts(message_hash_ptr, 32) };
    let publick_key = unsafe { slice::from_raw_parts(pubkey_ptr, 65) };
    match hex::decode(signature) {
        Ok(data) => verify_signature(&data, message_hash, publick_key).unwrap_or(false),
        Err(_) => false,
    }
}
