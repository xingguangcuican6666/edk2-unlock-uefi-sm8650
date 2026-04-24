#!/usr/bin/env python3
"""Repack a stock Android boot v4 image while preserving the original kernel."""

from __future__ import annotations

import argparse
import pathlib
import struct

BOOT_MAGIC = b"ANDROID!"
BOOT_HEADER_V4_SIZE = 1584
PAGE_SIZE = 4096


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--template-boot", required=True, help="Stock boot.img path")
    parser.add_argument("--signature-blob", help="Opaque blob to place into boot signature area")
    parser.add_argument("--output", required=True, help="Output boot image")
    return parser.parse_args()


def align(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


def main() -> int:
    args = parse_args()
    template = pathlib.Path(args.template_boot).read_bytes()
    output = pathlib.Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    if template[:8] != BOOT_MAGIC:
        raise ValueError("template boot image does not start with ANDROID!")

    kernel_size = struct.unpack_from("<I", template, 8)[0]
    ramdisk_size = struct.unpack_from("<I", template, 12)[0]
    header_size = struct.unpack_from("<I", template, 20)[0]
    header_version = struct.unpack_from("<I", template, 40)[0]
    if header_version != 4:
        raise ValueError("only boot header v4 templates are supported")
    if header_size != BOOT_HEADER_V4_SIZE:
        raise ValueError("unexpected boot header size in template")

    kernel_offset = PAGE_SIZE
    kernel_end = kernel_offset + align(kernel_size, PAGE_SIZE)
    kernel_blob = template[kernel_offset:kernel_offset + kernel_size]

    signature_blob = b""
    if args.signature_blob:
        signature_blob = pathlib.Path(args.signature_blob).read_bytes()

    image = bytearray(template[:PAGE_SIZE])
    image[kernel_offset:kernel_offset + len(kernel_blob)] = kernel_blob

    if len(image) < kernel_end:
        image.extend(b"\0" * (kernel_end - len(image)))
    image = image[:kernel_end]

    if signature_blob:
        struct.pack_into("<I", image, 44 + 1536, len(signature_blob))
        image.extend(signature_blob)
        image.extend(b"\0" * (align(len(signature_blob), PAGE_SIZE) - len(signature_blob)))
    else:
        struct.pack_into("<I", image, 44 + 1536, 0)

    output.write_bytes(bytes(image))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
