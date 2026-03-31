use std::ffi::{CStr, CString};
use std::os::raw::c_char;

fn extract_numeric_version(input: &str) -> Option<String> {
    let s = input.trim();
    let mut found = false;
    let mut version = String::new();

    for c in s.chars() {
        if !found {
            if c.is_ascii_digit() {
                found = true;
                version.push(c);
            }
        } else if c.is_ascii_digit() || c == '.' {
            version.push(c);
        } else {
            break;
        }
    }

    if version.is_empty() {
        None
    } else {
        Some(version)
    }
}

fn parse_parts(version: &str) -> Option<Vec<i64>> {
    let digits: Vec<_> = version
        .split('.')
        .map(|part| part.parse::<i64>())
        .collect();
    if digits.iter().any(|r| r.is_err()) {
        return None;
    }
    let values: Vec<i64> = digits.into_iter().map(|r| r.unwrap()).collect();
    if values.is_empty() {
        None
    } else {
        Some(values)
    }
}

fn canonicalize(input: &str) -> Option<String> {
    let numeric = extract_numeric_version(input)?;
    let parts = parse_parts(&numeric)?;
    Some(parts.iter().map(|v| v.to_string()).collect::<Vec<_>>().join("."))
}

fn compare_parts(a: &[i64], b: &[i64]) -> i32 {
    let max_len = std::cmp::max(a.len(), b.len());
    for i in 0..max_len {
        let aval = *a.get(i).unwrap_or(&0);
        let bval = *b.get(i).unwrap_or(&0);
        if aval < bval {
            return -1;
        }
        if aval > bval {
            return 1;
        }
    }
    0
}

#[no_mangle]
pub extern "C" fn vs_is_valid_version(input: *const c_char) -> u8 {
    if input.is_null() {
        return 0;
    }
    let cstr = unsafe { CStr::from_ptr(input) };
    if let Ok(text) = cstr.to_str() {
        extract_numeric_version(text)
            .and_then(|v| parse_parts(&v))
            .is_some() as u8
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn vs_normalize_version(input: *const c_char) -> *mut c_char {
    if input.is_null() {
        return std::ptr::null_mut();
    }
    let cstr = unsafe { CStr::from_ptr(input) };
    let text = match cstr.to_str() {
        Ok(t) => t,
        Err(_) => return std::ptr::null_mut(),
    };
    match canonicalize(text) {
        Some(norm) => CString::new(norm).unwrap().into_raw(),
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn vs_compare_versions(left: *const c_char, right: *const c_char) -> i32 {
    if left.is_null() || right.is_null() {
        return 2;
    }

    let left = unsafe { CStr::from_ptr(left) };
    let right = unsafe { CStr::from_ptr(right) };
    let left = match left.to_str() {
        Ok(s) => s,
        Err(_) => return 2,
    };
    let right = match right.to_str() {
        Ok(s) => s,
        Err(_) => return 2,
    };

    let left_norm = extract_numeric_version(left);
    let right_norm = extract_numeric_version(right);
    if left_norm.is_none() || right_norm.is_none() {
        return 2;
    }

    let left_parts = parse_parts(&left_norm.unwrap());
    let right_parts = parse_parts(&right_norm.unwrap());
    match (left_parts, right_parts) {
        (Some(a), Some(b)) => compare_parts(&a, &b),
        _ => 2,
    }
}

#[no_mangle]
pub extern "C" fn vs_free_string(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}
