#!/usr/bin/env python3
"""Build an ARM64 kernel-shaped probe shim with embedded OEM payload."""

from __future__ import annotations

import argparse
import gzip
import json
import pathlib
import shutil
import subprocess
import tempfile

MODE_MAP = {
    "entry-reset": 0,
    "copy-reset": 1,
    "jump-payload": 2,
}


ASM_TEMPLATE = r"""
    .section .text
    .global _start
    .type _start, %function
_start:
    b shim_start
    nop
    .quad 0
    .quad image_end - _start
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .ascii "ARMd"
    .word 0

shim_start:
    mov x24, #{mode}
    cmp x24, #0
    b.eq do_reset

    adr x19, payload_base
    ldr x20, [x19, #0x18]
    ldr x21, [x19, #0x20]
    add x21, x21, x19
    ldrh w22, [x19, #0x36]
    ldrh w23, [x19, #0x38]

1:
    cbz w23, 2f
    ldr w0, [x21]
    cmp w0, #1
    b.ne 5f
    ldr x1, [x21, #8]
    ldr x2, [x21, #24]
    ldr x3, [x21, #32]
    add x1, x19, x1

3:
    cbz x3, 4f
    ldrb w4, [x1], #1
    strb w4, [x2], #1
    subs x3, x3, #1
    b.ne 3b

4:
    ldr x5, [x21, #40]
    ldr x6, [x21, #32]
    subs x5, x5, x6
    b.le 5f
6:
    strb wzr, [x2], #1
    subs x5, x5, #1
    b.ne 6b

5:
    add x21, x21, x22
    subs w23, w23, #1
    b.ne 1b

2:
    cmp x24, #1
    b.eq do_reset
    br x20

do_reset:
    movz x0, #0x0009
    movk x0, #0x8400, lsl #16
    smc #0
7:
    wfe
    b 7b

    .balign 0x1000
payload_base:
    .incbin "{payload_path}"
image_end:
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--payload-elf", required=True, help="OEM ELF payload to embed and jump to")
    parser.add_argument("--mode", choices=sorted(MODE_MAP), required=True, help="Probe behavior")
    parser.add_argument("--output-prefix", required=True, help="Output prefix without extension")
    return parser.parse_args()


def run(cmd: list[str], cwd: pathlib.Path) -> None:
    subprocess.run(cmd, cwd=str(cwd), check=True)


def main() -> int:
    args = parse_args()
    payload = pathlib.Path(args.payload_elf).resolve()
    output_prefix = pathlib.Path(args.output_prefix)
    output_prefix.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="probe-shim-", dir="/tmp") as tmp_dir_name:
        tmp_dir = pathlib.Path(tmp_dir_name)
        asm_path = tmp_dir / "probe_shim.S"
        obj_path = tmp_dir / "probe_shim.o"
        elf_path = tmp_dir / "probe_shim.elf"
        raw_path = output_prefix.with_suffix(".raw.bin")
        gzip_path = output_prefix.with_suffix(".raw.bin.gz")
        layout_path = output_prefix.with_suffix(".layout.json")

        asm_path.write_text(
            ASM_TEMPLATE.format(
                mode=MODE_MAP[args.mode],
                payload_path=str(payload).replace("\\", "\\\\"),
            )
        )

        run(["aarch64-linux-gnu-gcc", "-c", str(asm_path), "-o", str(obj_path)], tmp_dir)
        run(["aarch64-linux-gnu-ld", "-Ttext=0", "-o", str(elf_path), str(obj_path)], tmp_dir)
        run(["aarch64-linux-gnu-objcopy", "-O", "binary", str(elf_path), str(raw_path)], tmp_dir)

    raw = raw_path.read_bytes()
    gzip_path.write_bytes(gzip.compress(raw, compresslevel=9, mtime=0))

    layout = {
        "mode": args.mode,
        "payload_elf": str(payload),
        "raw": raw_path.name,
        "gzip": gzip_path.name,
        "raw_size": len(raw),
        "gzip_size": len(gzip_path.read_bytes()),
    }
    layout_path.write_text(json.dumps(layout, indent=2) + "\n")
    print(gzip_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
