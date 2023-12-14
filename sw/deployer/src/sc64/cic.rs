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
    let rol = |a: u32, s: u32| u32::rotate_left(a, s);
    let ror = |a: u32, s: u32| u32::rotate_right(a, s);
    let sum = |a0: u32, a1: u32, a2: u32| {
        let prod = (a0 as u64).wrapping_mul(if a1 == 0 { a2 as u64 } else { a1 as u64 });
        let hi = ((prod >> 32) & 0xFFFFFFFF) as u32;
        let lo = (prod & 0xFFFFFFFF) as u32;
        let diff = hi.wrapping_sub(lo);
        return if diff == 0 { a0 } else { diff };
    };

    let init = add(mul(MAGIC, seed as u32), 1) ^ get(0);

    let mut buf = vec![init; 16];

    for i in 1..=1008 as u32 {
        let prev = get(i.saturating_sub(2));
        let data = get(i - 1);

        buf[0] = add(buf[0], sum(sub(1007, i), data, i));
        buf[1] = sum(buf[1], data, i);
        buf[2] = buf[2] ^ data;
        buf[3] = add(buf[3], sum(add(data, 5), MAGIC, i));
        buf[4] = add(buf[4], ror(data, prev & 0x1F));
        buf[5] = add(buf[5], rol(data, prev >> 27));
        buf[6] = if data < buf[6] {
            add(buf[3], buf[6]) ^ add(data, i)
        } else {
            add(buf[4], data) ^ buf[6]
        };
        buf[7] = sum(buf[7], rol(data, prev & 0x1F), i);
        buf[8] = sum(buf[8], ror(data, prev >> 27), i);
        buf[9] = if prev < data {
            sum(buf[9], data, i)
        } else {
            add(buf[9], data)
        };

        if i == 1008 {
            break;
        }

        let next = get(i);

        buf[10] = sum(add(buf[10], data), next, i);
        buf[11] = sum(buf[11] ^ data, next, i);
        buf[12] = add(buf[12], buf[8] ^ data);
        buf[13] = add(buf[13], add(ror(data, data & 0x1F), ror(next, next & 0x1F)));
        buf[14] = sum(
            sum(buf[14], ror(data, prev & 0x1F), i),
            ror(next, data & 0x1F),
            i,
        );
        buf[15] = sum(
            sum(buf[15], rol(data, prev >> 27), i),
            rol(next, data >> 27),
            i,
        );
    }

    let mut final_buf = vec![buf[0]; 4];

    for i in 0..16 as u32 {
        let data = buf[i as usize];

        final_buf[0] = add(final_buf[0], ror(data, data & 0x1F));
        final_buf[1] = if data < final_buf[0] {
            add(final_buf[1], data)
        } else {
            sum(final_buf[1], data, i)
        };
        final_buf[2] = if ((data & 0x02) >> 1) == (data & 0x01) {
            add(final_buf[2], data)
        } else {
            sum(final_buf[2], data, i)
        };
        final_buf[3] = if (data & 0x01) == 0x01 {
            final_buf[3] ^ data
        } else {
            sum(final_buf[3], data, i)
        };
    }

    let final_sum = sum(final_buf[0], final_buf[1], 16);
    let final_xor = final_buf[3] ^ final_buf[2];

    let checksum = (((final_sum & 0xFFFF) as u64) << 32) | (final_xor as u64);

    Ok(checksum)
}

pub fn sign_ipl3(ipl3: &[u8]) -> Result<(u8, u64), Error> {
    let known_seed_checksum_pairs = [
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
