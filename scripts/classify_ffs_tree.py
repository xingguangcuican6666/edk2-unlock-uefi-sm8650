#!/usr/bin/env python3
"""Classify FFS/section trees extracted from UEFI images or zip dumps."""

from __future__ import annotations

import argparse
import collections
import json
import pathlib
import re
import zipfile

GUID_RE = re.compile(
    r"^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$"
)

GENERIC_COMPONENTS = {
    "",
    "kernel",
    "kernel.dump",
    "body.bin",
    "header.bin",
    "info.txt",
    "unc_data.bin",
    "Padding",
    "Volume free space",
}

SECTION_NAMES = {
    "DXE dependency section",
    "PE32 image section",
    "TE image section",
    "Raw section",
    "UI section",
    "Version section",
    "Volume image section",
    "Padding",
    "Volume free space",
    "DXE apriori file",
    "PEI apriori file",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Zip file or extracted directory")
    parser.add_argument("--output", help="Optional JSON output path")
    return parser.parse_args()


def normalize_component(component: str) -> str:
    return re.sub(r"^\d+\s+", "", component).strip()


def is_guid(component: str) -> bool:
    return bool(GUID_RE.fullmatch(component))


def is_section(component: str) -> bool:
    return component in SECTION_NAMES or component.endswith("section")


def list_entries(source: pathlib.Path) -> list[tuple[str, int, bool]]:
    if source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            return [
                (info.filename.rstrip("/"), info.file_size, info.is_dir())
                for info in archive.infolist()
            ]

    entries = []
    for path in sorted(source.rglob("*")):
        rel = path.relative_to(source).as_posix()
        entries.append((rel, path.stat().st_size if path.is_file() else 0, path.is_dir()))
    return entries


def analyze_source(source: pathlib.Path) -> dict[str, object]:
    entries = list_entries(source)
    files = [(path, size) for path, size, is_dir in entries if not is_dir]
    top_levels = collections.Counter()
    extensions = collections.Counter()
    named_nodes = collections.Counter()
    section_dirs: set[str] = set()
    guid_nodes: set[str] = set()
    modules: dict[str, dict[str, object]] = {}

    root_prefix = None
    first_components = {path.split("/", 1)[0] for path, _, _ in entries if path}
    if len(first_components) == 1:
        root_prefix = next(iter(first_components))

    for rel_path, size in files:
        parts = rel_path.split("/")
        if root_prefix and parts and parts[0] == root_prefix:
            parts = parts[1:]
        normalized = [normalize_component(part) for part in parts]

        top_levels[parts[0] if parts else "<root>"] += 1
        extensions[pathlib.Path(rel_path).suffix.lower() or "<none>"] += 1

        for index, component in enumerate(normalized):
            if is_guid(component):
                guid_nodes.add(component)
            if (
                component not in GENERIC_COMPONENTS
                and not is_guid(component)
                and not is_section(component)
            ):
                named_nodes[component] += 1

            if not is_section(component):
                continue

            section_dirs.add("/".join(normalized[: index + 1]))
            candidate_index = index - 1
            while candidate_index >= 0:
                candidate = normalized[candidate_index]
                if (
                    not candidate
                    or is_guid(candidate)
                    or is_section(candidate)
                    or candidate in GENERIC_COMPONENTS
                ):
                    candidate_index -= 1
                    continue
                break

            if candidate_index < 0:
                continue

            module_name = normalized[candidate_index]
            module_entry = modules.setdefault(
                module_name,
                {"paths": set(), "sections": set(), "bytes": 0},
            )
            module_entry["paths"].add("/".join(normalized[: candidate_index + 1]))
            module_entry["sections"].add(component)
            if normalized[-1] == "body.bin":
                module_entry["bytes"] += size

    normalized_modules = {
        name: {
            "paths": sorted(value["paths"]),
            "sections": sorted(value["sections"]),
            "bytes": value["bytes"],
        }
        for name, value in sorted(modules.items())
    }

    summary = {
        "input": str(source),
        "root_prefix": root_prefix,
        "entry_count": len(entries),
        "file_count": len(files),
        "total_uncompressed_size": sum(size for _, size in files),
        "top_levels": top_levels.most_common(),
        "extensions": extensions.most_common(),
        "guid_count": len(guid_nodes),
        "guids": sorted(guid_nodes),
        "section_count": len(section_dirs),
        "sections": sorted(section_dirs),
        "named_nodes": named_nodes.most_common(200),
        "module_count": len(normalized_modules),
        "modules": normalized_modules,
    }
    return summary


def main() -> int:
    args = parse_args()
    source = pathlib.Path(args.input)
    summary = analyze_source(source)
    text = json.dumps(summary, indent=2) + "\n"
    if args.output:
        pathlib.Path(args.output).write_text(text)
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
