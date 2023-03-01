use crate::sc64::DebugPacket;

pub fn handle_debug_packet(debug_packet: DebugPacket) {
    let DebugPacket { datatype, data } = debug_packet;
    match datatype {
        0x01 => handle_datatype_text(&data),
        // 0x02 => handle_datatype_raw_binary(&data),
        // 0x03 => handle_datatype_header(&data),
        // 0x04 => handle_datatype_screenshot(&data),
        // 0xDB => handle_datatype_gdb(&data),
        _ => {}
    }
}

fn handle_datatype_text(data: &[u8]) {
    if let Ok(message) = std::str::from_utf8(data) {
        print!("{message}");
    }
}

// fn handle_datatype_raw_binary(data: &[u8]) {}

// fn handle_datatype_header(data: &[u8]) {}

// fn handle_datatype_screenshot(data: &[u8]) {}

// fn handle_datatype_gdb(data: &[u8]) {}
