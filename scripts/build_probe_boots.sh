#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_DIR="${1:-${ROOT_DIR}/out/artifacts}"

PAYLOAD_ELF="${ROOT_DIR}/imgs/uefi.elf"
TEMPLATE_BOOT="${ROOT_DIR}/imgs/8e.img"

mkdir -p "${ARTIFACT_DIR}/probes" "${ARTIFACT_DIR}/oem"

build_one() {
  local mode="$1"
  local prefix="${ARTIFACT_DIR}/probes/pineapple-${mode}"
  python3 "${ROOT_DIR}/scripts/build_probe_kernel_shim.py" \
    --payload-elf "${PAYLOAD_ELF}" \
    --mode "${mode}" \
    --output-prefix "${prefix}"
  python3 "${ROOT_DIR}/scripts/repack_8e_style_boot.py" \
    --template-8e "${TEMPLATE_BOOT}" \
    --kernel-gzip "${prefix}.raw.bin.gz" \
    --output "${prefix}.boot.img"
  python3 "${ROOT_DIR}/scripts/analyze_kernel_shim_layout.py" \
    --input "${prefix}.boot.img" \
    --output-dir "${ARTIFACT_DIR}/oem"
  rm -f \
    "${ARTIFACT_DIR}/oem/$(basename "${prefix}").boot.kernel.bin" \
    "${ARTIFACT_DIR}/oem/$(basename "${prefix}").boot.kernel.unpacked.bin"
}

build_one "entry-reset"
build_one "copy-reset"
build_one "jump-payload"
