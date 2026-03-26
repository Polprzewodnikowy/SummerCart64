#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path


def _c_ident_from_path(path: Path) -> str:
    raw = path.name
    out: list[str] = []
    for ch in raw:
        if ch.isalnum():
            out.append(ch)
        else:
            out.append("_")
    return "m_TrxLua_" + "".join(out)


def _bytes_to_c_array(data: bytes) -> str:
    per_line = 16
    chunks: list[str] = []
    for i in range(0, len(data), per_line):
        chunk = ", ".join(f"0x{b:02x}" for b in data[i : i + per_line])
        chunks.append("    " + chunk)
    return ",\n".join(chunks)


def main() -> int:
    parser = argparse.ArgumentParser(description="Embed TRX Lua scripts into C")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("inputs", nargs="+", type=Path)
    args = parser.parse_args()

    output_path: Path = args.output
    inputs: list[Path] = list(args.inputs)

    scripts: list[tuple[str, str, bytes]] = []
    for in_path in inputs:
        data = in_path.read_bytes()
        c_ident = _c_ident_from_path(in_path)
        scripts.append((in_path.name, c_ident, data))

    lines: list[str] = []
    lines.append("// Auto-generated file; do not edit.")
    lines.append('#include <trx/game/lua/embedded_scripts.h>')
    lines.append("")
    for name, c_ident, data in scripts:
        lines.append(f"static const uint8_t {c_ident}[] = {{")
        lines.append(_bytes_to_c_array(data) + ",")
        lines.append("};")
        lines.append("")

    lines.append("const LUA_EMBEDDED_SCRIPT g_LUA_EmbeddedScripts[] = {")
    for name, c_ident, data in scripts:
        lines.append(
            f'    {{ .path = "{name}", .data = {c_ident}, .size = {len(data)} }},'
        )
    lines.append("    { .path = nullptr, .data = nullptr, .size = 0 },")
    lines.append("};")
    lines.append("")

    output_path.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
