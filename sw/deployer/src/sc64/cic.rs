use super::Error;

pub const IPL3_OFFSET: u32 = 0x40;
pub const IPL3_LENGTH: usize = 0xFC0;

fn calculate_ipl3_checksum(ipl3: &[u8], seed: u8) -> Result<u64, Error> {
    if ipl3.len() < IPL3_LENGTH {
        return Err(Error::new("Invalid IPL3 length provided"));
    }

    const MAGIC: u32 = 0x6C078965;

    let get = |offset: u32| {
        let o: usize = offset as usize * 4;
        return ((ipl3[o] as u32) << 24)
            | ((ipl3[o + 1] as u32) << 16)
            | ((ipl3[o + 2] as u32) << 8)
            | (ipl3[o + 3] as u32);
    };
    let add = |a1: u32, a2: u32| u32::wrapping_add(a1, a2);
    let sub = |a1: u32, a2: u32| u32::wrapping_sub(a1, a2);
    let mul = |a1: u32, a2: u32| u32::wrapping_mul(a1, a2);
    let lsh = |a: u32, s: u32| if s >= 32 { 0 } else { u32::wrapping_shl(a, s) };
    let rsh = |a: u32, s: u32| if s >= 32 { 0 } else { u32::wrapping_shr(a, s) };
    let checksum = |a0: u32, a1: u32, a2: u32| {
        let prod = (a0 as u64).wrapping_mul(if a1 == 0 { a2 as u64 } else { a1 as u64 });
        let hi = ((prod >> 32) & 0xFFFFFFFF) as u32;
        let lo = (prod & 0xFFFFFFFF) as u32;
        let diff = hi.wrapping_sub(lo);
        return if diff == 0 { a0 } else { diff };
    };

    let init = add(mul(MAGIC, seed as u32), 1) ^ get(0);

    let mut buffer = vec![init; 16];

    for i in 1..=1008 as u32 {
        let data_prev = get(i.saturating_sub(2));
        let data_curr = get(i - 1);

        buffer[0] = add(buffer[0], checksum(sub(1007, i), data_curr, i));
        buffer[1] = checksum(buffer[1], data_curr, i);
        buffer[2] = buffer[2] ^ data_curr;
        buffer[3] = add(buffer[3], checksum(add(data_curr, 5), MAGIC, i));

        let shift = data_prev & 0x1F;
        let data_left = lsh(data_curr, 32 - shift);
        let data_right = rsh(data_curr, shift);
        let b4_shifted = data_left | data_right;
        buffer[4] = add(buffer[4], b4_shifted);

        let shift = rsh(data_prev, 27);
        let data_left = lsh(data_curr, shift);
        let data_right = rsh(data_curr, 32 - shift);
        let b5_shifted = data_left | data_right;
        buffer[5] = add(buffer[5], b5_shifted);

        if data_curr < buffer[6] {
            buffer[6] = add(buffer[3], buffer[6]) ^ add(data_curr, i);
        } else {
            buffer[6] = add(buffer[4], data_curr) ^ buffer[6];
        }

        let shift = data_prev & 0x1F;
        let data_left = lsh(data_curr, shift);
        let data_right = rsh(data_curr, 32 - shift);
        buffer[7] = checksum(buffer[7], data_left | data_right, i);

        let shift = rsh(data_prev, 27);
        let data_left = lsh(data_curr, 32 - shift);
        let data_right = rsh(data_curr, shift);
        buffer[8] = checksum(buffer[8], data_left | data_right, i);

        if data_prev < data_curr {
            buffer[9] = checksum(buffer[9], data_curr, i)
        } else {
            buffer[9] = add(buffer[9], data_curr);
        }

        if i == 1008 {
            break;
        }

        let data_next = get(i);

        buffer[10] = checksum(add(buffer[10], data_curr), data_next, i);
        buffer[11] = checksum(buffer[11] ^ data_curr, data_next, i);
        buffer[12] = add(buffer[12], buffer[8] ^ data_curr);

        let shift = data_curr & 0x1F;
        let data_left = lsh(data_curr, 32 - shift);
        let data_right = rsh(data_curr, shift);
        let tmp = data_left | data_right;
        let shift = data_next & 0x1F;
        let data_left = lsh(data_next, 32 - shift);
        let data_right = rsh(data_next, shift);
        buffer[13] = add(buffer[13], add(tmp, data_left | data_right));

        let shift = data_curr & 0x1F;
        let data_left = lsh(data_next, 32 - shift);
        let data_right = rsh(data_next, shift);
        let sum = checksum(buffer[14], b4_shifted, i);
        buffer[14] = checksum(sum, data_left | data_right, i);

        let shift = rsh(data_curr, 27);
        let data_left = lsh(data_next, shift);
        let data_right = rsh(data_next, 32 - shift);
        let sum = checksum(buffer[15], b5_shifted, i);
        buffer[15] = checksum(sum, data_left | data_right, i);
    }

    let mut final_buffer = vec![buffer[0]; 4];

    for i in 0..16 as u32 {
        let data = buffer[i as usize];

        let shift = data & 0x1F;
        let data_left = lsh(data, 32 - shift);
        let data_right = rsh(data, shift);
        let b0_shifted = add(final_buffer[0], data_left | data_right);
        final_buffer[0] = b0_shifted;

        if data < b0_shifted {
            final_buffer[1] = add(final_buffer[1], data);
        } else {
            final_buffer[1] = checksum(final_buffer[1], data, i);
        }

        if rsh(data & 0x02, 1) == (data & 0x01) {
            final_buffer[2] = add(final_buffer[2], data);
        } else {
            final_buffer[2] = checksum(final_buffer[2], data, i);
        }

        if (data & 0x01) == 0x01 {
            final_buffer[3] = final_buffer[3] ^ data;
        } else {
            final_buffer[3] = checksum(final_buffer[3], data, i);
        }
    }

    let sum = checksum(final_buffer[0], final_buffer[1], 16);
    let xor = final_buffer[3] ^ final_buffer[2];

    let checksum = (((sum & 0xFFFF) as u64) << 32) | (xor as u64);

    Ok(checksum)
}

pub fn sign_ipl3(ipl3: &[u8]) -> Result<(u8, u64), Error> {
    let known_seed_checksum_pairs = [
        (0xAC, 0x5930D81014DAu64), // 5101
        (0xDD, 0x083C6C77E0B1u64), // 5167
        (0x3F, 0x45CC73EE317Au64), // 6101
        (0x3F, 0x44160EC5D9AFu64), // 7102
        (0x3F, 0xA536C0F1D859u64), // 6102/7102
        (0x78, 0x586FD4709867u64), // 6103/7103
        (0x91, 0x8618A45BC2D3u64), // 6105/7105
        (0x85, 0x2BBAD4E6EB74u64), // 6106/7106
        (0xDD, 0x6EE8D9E84970u64), // NDXJ0
        (0xDD, 0x6C216495C8B9u64), // NDDJ0
        (0xDD, 0xE27F43BA93ACu64), // NDDJ1
        (0xDD, 0x32B294E2AB90u64), // NDDJ2
        (0xDE, 0x05BA2EF0A5F1u64), // NDDE0
    ];

    for (seed, checksum) in known_seed_checksum_pairs {
        if calculate_ipl3_checksum(ipl3, seed)? == checksum {
            return Ok((seed, checksum));
        }
    }

    // Unknown IPL3 detected, sign it with arbitrary seed (CIC6102/7101 value is used here)
    const DEFAULT_SEED: u8 = 0x3F;
    let checksum = calculate_ipl3_checksum(ipl3, DEFAULT_SEED)?;

    Ok((DEFAULT_SEED, checksum))
}
