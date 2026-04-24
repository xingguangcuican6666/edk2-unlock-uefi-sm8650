#!/usr/bin/env python3
"""Compare two UEFI dump zip trees and generate a concise diff report."""

from __future__ import annotations

import argparse
import json
import pathlib

from classify_ffs_tree import analyze_source


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", required=True, help="Reference zip/directory, e.g. 8e")
    parser.add_argument("--candidate", required=True, help="Candidate zip/directory, e.g. 8gen3")
    parser.add_argument("--output-dir", required=True, help="Directory for reports")
    return parser.parse_args()


def write_text(path: pathlib.Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def main() -> int:
    args = parse_args()
    out_dir = pathlib.Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    reference = analyze_source(pathlib.Path(args.reference))
    candidate = analyze_source(pathlib.Path(args.candidate))

    ref_modules = reference["modules"]
    cand_modules = candidate["modules"]
    ref_module_names = set(ref_modules)
    cand_module_names = set(cand_modules)

    ref_only = sorted(ref_module_names - cand_module_names)
    cand_only = sorted(cand_module_names - ref_module_names)
    shared = sorted(ref_module_names & cand_module_names)

    ranked_ref_only = sorted(
        (
            {
                "name": name,
                "bytes": ref_modules[name]["bytes"],
                "sections": ref_modules[name]["sections"],
            }
            for name in ref_only
        ),
        key=lambda item: (-item["bytes"], item["name"]),
    )

    report = {
        "reference": reference["input"],
        "candidate": candidate["input"],
        "reference_entry_count": reference["entry_count"],
        "candidate_entry_count": candidate["entry_count"],
        "reference_module_count": reference["module_count"],
        "candidate_module_count": candidate["module_count"],
        "reference_only_modules": ranked_ref_only,
        "candidate_only_modules": cand_only,
        "shared_modules": shared,
        "reference_top_levels": reference["top_levels"],
        "candidate_top_levels": candidate["top_levels"],
        "reference_extensions": reference["extensions"],
        "candidate_extensions": candidate["extensions"],
    }

    write_text(out_dir / "summary.json", json.dumps(report, indent=2) + "\n")
    write_text(out_dir / "reference.summary.json", json.dumps(reference, indent=2) + "\n")
    write_text(out_dir / "candidate.summary.json", json.dumps(candidate, indent=2) + "\n")
    write_text(out_dir / "reference_only_modules.txt", "\n".join(ref_only) + ("\n" if ref_only else ""))
    write_text(out_dir / "candidate_only_modules.txt", "\n".join(cand_only) + ("\n" if cand_only else ""))

    lines = [
        "# 8e vs 8gen3 UEFI Dump Comparison",
        "",
        f"- Reference: `{reference['input']}`",
        f"- Candidate: `{candidate['input']}`",
        f"- Reference entries: `{reference['entry_count']}`",
        f"- Candidate entries: `{candidate['entry_count']}`",
        f"- Reference modules: `{reference['module_count']}`",
        f"- Candidate modules: `{candidate['module_count']}`",
        "",
        "## Key Gap",
        "",
        f"- Modules present only in reference: `{len(ref_only)}`",
        f"- Modules present only in candidate: `{len(cand_only)}`",
        f"- Shared modules: `{len(shared)}`",
        "",
        "## Largest Reference-Only Modules",
        "",
    ]

    for item in ranked_ref_only[:80]:
        sections = ", ".join(item["sections"])
        lines.append(f"- `{item['name']}`: {item['bytes']} bytes, sections: {sections}")

    lines.extend(
        [
            "",
            "## Candidate-Only Modules",
            "",
        ]
    )
    for name in cand_only[:80]:
        lines.append(f"- `{name}`")

    lines.extend(
        [
            "",
            "## Top-Level Layout",
            "",
            f"- Reference top levels: `{reference['top_levels']}`",
            f"- Candidate top levels: `{candidate['top_levels']}`",
            "",
            "## Extension Mix",
            "",
            f"- Reference extensions: `{reference['extensions']}`",
            f"- Candidate extensions: `{candidate['extensions']}`",
            "",
        ]
    )

    write_text(out_dir / "summary.md", "\n".join(lines) + "\n")
    print(out_dir / "summary.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
