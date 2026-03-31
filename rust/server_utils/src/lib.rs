use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;

fn parse_cstr<'a>(ptr: *const c_char) -> Option<&'a str> {
    if ptr.is_null() {
        return None;
    }
    let cstr = unsafe { CStr::from_ptr(ptr) };
    cstr.to_str().ok()
}

#[no_mangle]
pub extern "C" fn rs_validate_repo_name(input: *const c_char) -> u8 {
    let text = match parse_cstr(input) {
        Some(s) => s,
        None => return 0,
    };
    if text.is_empty() {
        return 0;
    }
    for c in text.chars() {
        if !c.is_ascii_alphanumeric() && c != '-' && c != '_' && c != '.' {
            return 0;
        }
    }
    1
}

#[no_mangle]
pub extern "C" fn rs_list_repositories(repo_dir: *const c_char) -> *mut c_char {
    let dir = match parse_cstr(repo_dir) {
        Some(s) => s,
        None => return std::ptr::null_mut(),
    };
    let path = Path::new(dir);
    let mut names = Vec::new();
    if path.exists() && path.is_dir() {
        if let Ok(entries) = path.read_dir() {
            for entry in entries.flatten() {
                let entry_path = entry.path();
                if entry_path.is_dir() && entry_path.join(".nvcs").exists() {
                    if let Some(name) = entry_path.file_name().and_then(|n| n.to_str()) {
                        names.push(name.to_string());
                    }
                }
            }
        }
    }
    let output = names.join("\n");
    match CString::new(output) {
        Ok(cstring) => cstring.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn rs_free_string(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}
