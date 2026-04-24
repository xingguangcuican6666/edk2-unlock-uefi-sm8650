#!/usr/bin/env python3
"""Pack a minimal Android boot.img that carries a UEFI payload as kernel."""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys

BOOT_MAGIC = b"ANDROID!"
BOOT_ARGS_SIZE = 512
BOOT_EXTRA_ARGS_SIZE = 1024
BOOT_HEADER_V4_SIZE = 1584
BOOT_IMAGE_PAGE_SIZE = 4096
CMDLINE_SIZE = BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", required=True, help="Kernel payload to embed")
    parser.add_argument("--output", required=True, help="Output boot.img path")
    parser.add_argument(
        "--header-version",
        type=int,
        default=4,
        help="Android boot header version. Only v4 is currently supported.",
    )
    parser.add_argument(
        "--cmdline",
        default="",
        help="Kernel cmdline stored in the boot header",
    )
    parser.add_argument(
        "--os-version",
        default="0.0.0",
        help="OS version encoded into the header, for example 14.0.0",
    )
    parser.add_argument(
        "--os-patch-level",
        default="2000-00",
        help="OS patch level encoded into the header, for example 2026-04",
    )
    return parser.parse_args()


def align(size: int, alignment: int) -> int:
    return (size + alignment - 1) // alignment * alignment


def encode_cmdline(cmdline: str) -> bytes:
    encoded = cmdline.encode("ascii")
    if len(encoded) >= CMDLINE_SIZE:
        raise ValueError(
            f"cmdline is too long ({len(encoded)} bytes, max {CMDLINE_SIZE - 1})"
        )
    return encoded + b"\0" + (b"\0" * (CMDLINE_SIZE - len(encoded) - 1))


def encode_os_version(version: str, patch_level: str) -> int:
    if version in ("", "0", "0.0", "0.0.0") and patch_level in ("", "0", "2000-00"):
        return 0

    try:
        major, minor, patch = (int(part) for part in version.split("."))
        year, month = (int(part) for part in patch_level.split("-"))
    except ValueError as exc:
        raise ValueError("invalid os version or patch level") from exc

    if not (0 <= major < 128 and 0 <= minor < 128 and 0 <= patch < 128):
        raise ValueError("OS version fields must fit in 7 bits")
    if not (2000 <= year < 2128 and 0 <= month < 16):
        raise ValueError("OS patch level must encode year >= 2000 and month < 16")

    return (
        (major << 25)
        | (minor << 18)
        | (patch << 11)
        | ((year - 2000) << 4)
        | month
    )


def pack_v4_image(kernel: bytes, cmdline: bytes, os_version: int) -> bytes:
    header = struct.pack(
        "<8sIIII4II1536sI",
        BOOT_MAGIC,
        len(kernel),
        0,
        os_version,
        BOOT_HEADER_V4_SIZE,
        0,
        0,
        0,
        0,
        4,
        cmdline,
        0,
    )
    if len(header) != BOOT_HEADER_V4_SIZE:
        raise ValueError(f"unexpected boot header size {len(header)}")

    image = bytearray()
    image += header
    image += b"\0" * (BOOT_IMAGE_PAGE_SIZE - len(header))
    image += kernel
    image += b"\0" * (align(len(kernel), BOOT_IMAGE_PAGE_SIZE) - len(kernel))
    return bytes(image)


def main() -> int:
    args = parse_args()
    if args.header_version != 4:
        raise ValueError("only boot header v4 is supported by this packer")

    kernel = pathlib.Path(args.kernel).read_bytes()
    cmdline = encode_cmdline(args.cmdline)
    os_version = encode_os_version(args.os_version, args.os_patch_level)

    output = pathlib.Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(pack_v4_image(kernel, cmdline, os_version))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
