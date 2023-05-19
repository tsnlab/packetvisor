use crate::pv::PvPacket;
use pnet::datalink::MacAddr;

// return u16 that gets 2 bytes from const* u8
pub fn endian16(ptr: *const u8) -> u16 {
    (unsafe { *(ptr.offset(1)) } as u16) << 8 | unsafe { *ptr } as u16
}

// return u32 that gets 4 bytes from const* u8
pub fn endian32(ptr: *const u8) -> u32 {
    unsafe {
        ((*ptr.offset(3) as u32) << 24)
            | ((*ptr.offset(2) as u32) << 16)
            | ((*ptr.offset(1) as u32) << 8)
            | ((*ptr.offset(0) as u32) << 0)
    }
}

// return u64 that gets 6 bytes from const* u8
pub fn endian48(ptr: *const u8) -> u64 {
    unsafe {
        ((*ptr.offset(5) as u64) << 40)
            | ((*ptr.offset(4) as u64) << 32)
            | ((*ptr.offset(3) as u64) << 24)
            | ((*ptr.offset(2) as u64) << 16)
            | ((*ptr.offset(1) as u64) << 8)
            | ((*ptr.offset(0) as u64) << 0)
    }
}

// return u64 that gets 8 bytes from const* u8
pub fn endian64(ptr: *const u8) -> u64 {
    unsafe {
        ((*ptr.offset(7) as u64) << 56)
            | ((*ptr.offset(6) as u64) << 48)
            | ((*ptr.offset(5) as u64) << 40)
            | ((*ptr.offset(4) as u64) << 32)
            | ((*ptr.offset(3) as u64) << 24)
            | ((*ptr.offset(2) as u64) << 16)
            | ((*ptr.offset(1) as u64) << 8)
            | ((*ptr.offset(0) as u64) << 0)
    }
}

// return u64 that gets n bytes from const* u8
pub fn endian(ptr: *const u8, size: u32) -> u64 {
    unsafe {
        let mut ret: u64 = 0;
        let mut n = 0;
        while size > n {
            ret |= (*ptr.offset(n as isize) as u64) << (n * 8);
            n += 1;
        }
        ret
    }
}

// makes MacAddr to u64
pub fn macAddr_to_u64(mac: &MacAddr) -> u64 {
    let mut ret: u64 = 0;
    let mut n = 0;

    let mac: [u8; 6] = mac.octets();
    for i in mac.iter() {
        ret |= (*i as u64) << (n * 8);
        n += 1;
    }
    ret
}

pub fn set_bits(ptr: *mut u8, value: u64, len: u32) {
    let mut n = 0;
    unsafe {
        while len > n {
            *ptr.offset(n as isize) = (value >> (n * 8)) as u8;
            n += 1;
        }
    }
}

pub fn check(packet: &PvPacket, position: u32, size: u32) -> Result<u8, u32> {
    if packet.start + position + size > packet.end {
        Err(1)
    } else {
        Ok(0)
    }
}

pub fn get(packet: &PvPacket, position: u32, size: u32) -> Result<u64, u32> {
    match check(packet, position, size) {
        Ok(_) => Ok(endian(
            unsafe { packet.buffer.offset((packet.start + position) as isize) },
            size,
        )),
        Err(_) => Err(1),
    }
}

pub fn set(packet: &mut PvPacket, position: u32, size: u32, value: u64) -> Result<&PvPacket, u32> {
    match check(packet, position, size) {
        Ok(_) => {
            unsafe {
                set_bits(packet.buffer.offset((packet.start + position) as isize), value, size);
            };
            Ok(packet)
        }
        Err(_) => Err(2),
    }
}
