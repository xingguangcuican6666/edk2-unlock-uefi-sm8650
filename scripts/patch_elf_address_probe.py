#!/usr/bin/env python3
"""Patch a specific AArch64 ELF virtual address with a small probe stub."""

from __future__ import annotations

import argparse
import pathlib
import struct
import subprocess
import tempfile


MODE_TO_ASM = {
    "loop": """
        .text
        .global _start
    _start:
    1:
        wfe
        b 1b
    """,
    "reset": """
        .text
        .global _start
    _start:
        movz x0, #0x0009
        movk x0, #0x8400, lsl #16
        smc #0
    1:
        wfe
        b 1b
    """,
    "delay-reset": """
        .text
        .global _start
    _start:
        movz x1, #0xffff
        movk x1, #0x00ff, lsl #16
    1:
        subs x1, x1, #1
        b.ne 1b
        movz x0, #0x0009
        movk x0, #0x8400, lsl #16
        smc #0
    2:
        wfe
        b 2b
    """,
}

LOAD_TYPE = 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Input ELF path")
    parser.add_argument("--address", required=True, help="Virtual address to patch")
    parser.add_argument("--mode", choices=sorted(MODE_TO_ASM), required=True, help="Probe behavior")
    parser.add_argument("--output", required=True, help="Patched ELF output path")
    return parser.parse_args()


def parse_int(value: str) -> int:
    return int(value, 0)


def vaddr_file_offset(data: bytes, vaddr: int) -> int:
    elf_class = data[4]
    if elf_class != 2:
      raise ValueError("only ELF64 payloads are supported")

    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data, 0)
    e_phoff = header[5]
    e_phentsize = header[9]
    e_phnum = header[10]
    ph_fmt = "<IIQQQQQQ"

    for index in range(e_phnum):
        offset = e_phoff + index * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(ph_fmt, data, offset)
        if p_type != LOAD_TYPE:
            continue
        if p_vaddr <= vaddr < p_vaddr + p_filesz:
            return p_offset + (vaddr - p_vaddr)

    raise ValueError(f"virtual address {hex(vaddr)} is not inside a LOAD segment")


def build_stub(mode: str) -> bytes:
    with tempfile.TemporaryDirectory(prefix="elf-addr-probe-", dir="/tmp") as tmp_dir_name:
        tmp_dir = pathlib.Path(tmp_dir_name)
        asm_path = tmp_dir / "probe.S"
        obj_path = tmp_dir / "probe.o"
        bin_path = tmp_dir / "probe.bin"
        asm_path.write_text(MODE_TO_ASM[mode])
        subprocess.run(["aarch64-linux-gnu-gcc", "-c", str(asm_path), "-o", str(obj_path)], check=True, cwd=tmp_dir)
        subprocess.run(["aarch64-linux-gnu-objcopy", "-O", "binary", str(obj_path), str(bin_path)], check=True, cwd=tmp_dir)
        return bin_path.read_bytes()


def main() -> int:
    args = parse_args()
    vaddr = parse_int(args.address)
    input_path = pathlib.Path(args.input)
    output_path = pathlib.Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    data = bytearray(input_path.read_bytes())
    patch_offset = vaddr_file_offset(data, vaddr)
    stub = build_stub(args.mode)
    data[patch_offset:patch_offset + len(stub)] = stub
    output_path.write_bytes(data)
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
