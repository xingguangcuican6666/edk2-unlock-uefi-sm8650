#!/usr/bin/env python3
"""Extract and summarize Qualcomm-style ELF payloads that embed UEFI FVs."""

from __future__ import annotations

import argparse
import json
import pathlib
import struct
import uuid

FVH_MAGIC = b"_FVH"
LOAD_TYPE = 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Input ELF path")
    parser.add_argument("--output-dir", required=True, help="Directory for extracted data")
    return parser.parse_args()


def align(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


def read_program_headers(data: bytes) -> tuple[int, int, list[dict[str, int]]]:
    elf_class = data[4]
    if elf_class == 1:
      header_fmt = "<16sHHIIIIIHHHHHH"
      ph_fmt = "<IIIIIIII"
      e_phoff = struct.unpack_from(header_fmt, data, 0)[5]
      e_phentsize = struct.unpack_from(header_fmt, data, 0)[9]
      e_phnum = struct.unpack_from(header_fmt, data, 0)[10]
    elif elf_class == 2:
      header_fmt = "<16sHHIQQQIHHHHHH"
      ph_fmt = "<IIQQQQQQ"
      e_phoff = struct.unpack_from(header_fmt, data, 0)[5]
      e_phentsize = struct.unpack_from(header_fmt, data, 0)[9]
      e_phnum = struct.unpack_from(header_fmt, data, 0)[10]
    else:
      raise ValueError("unsupported ELF class")

    headers = []
    for index in range(e_phnum):
      offset = e_phoff + index * e_phentsize
      if elf_class == 1:
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from(ph_fmt, data, offset)
      else:
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(ph_fmt, data, offset)
      headers.append(
        {
          "type": p_type,
          "offset": p_offset,
          "vaddr": p_vaddr,
          "paddr": p_paddr,
          "filesz": p_filesz,
          "memsz": p_memsz,
          "flags": p_flags,
          "align": p_align,
        }
      )
    return elf_class, e_phnum, headers


def parse_fv_header(data: bytes, fvh_offset: int) -> dict[str, object]:
    if fvh_offset < 0x28:
      raise ValueError("FVH offset too small for Qualcomm prefix")

    prefix = data[fvh_offset - 0x28:fvh_offset]
    file_system_guid = str(uuid.UUID(bytes_le=data[fvh_offset - 0x18:fvh_offset - 0x08]))
    fv_length = struct.unpack_from("<Q", data, fvh_offset - 0x08)[0]
    attributes = struct.unpack_from("<I", data, fvh_offset + 4)[0]
    header_length = struct.unpack_from("<H", data, fvh_offset + 8)[0]
    checksum = struct.unpack_from("<H", data, fvh_offset + 10)[0]
    ext_header_offset = struct.unpack_from("<H", data, fvh_offset + 12)[0]
    revision = data[fvh_offset + 15]

    block_map = []
    cursor = fvh_offset + 16
    while cursor + 8 <= len(data):
      block_size, block_count = struct.unpack_from("<II", data, cursor)
      if block_size == 0 and block_count == 0:
        break
      block_map.append({"block_size": block_size, "block_count": block_count})
      cursor += 8

    return {
      "qualcomm_prefix_offset": fvh_offset - 0x28,
      "fvh_offset": fvh_offset,
      "qualcomm_prefix_hex": prefix.hex(),
      "file_system_guid": file_system_guid,
      "fv_length": fv_length,
      "attributes": attributes,
      "header_length": header_length,
      "checksum": checksum,
      "ext_header_offset": ext_header_offset,
      "revision": revision,
      "block_map": block_map,
    }


def parse_ffs_entries(fv_data: bytes, fv_base_offset: int, header_length: int, fv_length: int) -> list[dict[str, object]]:
    entries = []
    cursor = align(header_length, 8)
    limit = min(len(fv_data), fv_length)
    while cursor + 24 <= limit:
      header = fv_data[cursor:cursor + 24]
      if header == b"\xff" * 24:
        break
      file_guid = str(uuid.UUID(bytes_le=header[:16]))
      file_type = header[18]
      attributes = header[19]
      size = header[20] | (header[21] << 8) | (header[22] << 16)
      state = header[23]
      if size == 0xFFFFFF or size < 24 or cursor + size > limit:
        break
      if file_type == 0 or state == 0:
        break
      entries.append(
        {
          "offset": fv_base_offset + cursor,
          "guid": file_guid,
          "type": file_type,
          "attributes": attributes,
          "size": size,
          "state": state,
        }
      )
      cursor = align(cursor + size, 8)
    return entries


def main() -> int:
    args = parse_args()
    input_path = pathlib.Path(args.input)
    output_dir = pathlib.Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    data = input_path.read_bytes()
    elf_class, ph_num, headers = read_program_headers(data)
    load_segments = [segment for segment in headers if segment["type"] == LOAD_TYPE and segment["filesz"] > 0]

    fv_hits = []
    for index, segment in enumerate(load_segments):
      segment_data = data[segment["offset"]:segment["offset"] + segment["filesz"]]
      local_offset = segment_data.find(FVH_MAGIC)
      if local_offset < 0:
        continue
      fvh_offset = segment["offset"] + local_offset
      header = parse_fv_header(data, fvh_offset)
      fv_blob = data[header["qualcomm_prefix_offset"]:header["qualcomm_prefix_offset"] + header["fv_length"]]
      fv_path = output_dir / f"{input_path.stem}.segment{index}.fv.bin"
      fv_path.write_bytes(fv_blob)
      header["extracted_fv"] = fv_path.name
      header["ffs_entries"] = parse_ffs_entries(
        fv_blob[0x28:],
        header["qualcomm_prefix_offset"] + 0x28,
        header["header_length"],
        header["fv_length"] - 0x28,
      )
      fv_hits.append(header)

    summary = {
      "input": str(input_path),
      "size": len(data),
      "elf_class": 32 if elf_class == 1 else 64,
      "program_headers": ph_num,
      "load_segments": load_segments,
      "fv_hits": fv_hits,
    }

    manifest_path = output_dir / f"{input_path.stem}.manifest.json"
    manifest_path.write_text(json.dumps(summary, indent=2) + "\n")
    print(manifest_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
