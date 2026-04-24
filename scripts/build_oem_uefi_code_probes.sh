#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_DIR="${1:-${ROOT_DIR}/out/artifacts}"
ARTIFACT_DIR="$(python3 -c 'import pathlib,sys; print(pathlib.Path(sys.argv[1]).resolve())' "${ARTIFACT_DIR}")"
PAYLOAD="${ROOT_DIR}/imgs/uefi.elf"
TEMPLATE_BOOT="${ROOT_DIR}/imgs/8e.img"

mkdir -p "${ARTIFACT_DIR}/oem-uefi-probes" "${ARTIFACT_DIR}/oem"

build_one() {
  local probe_name="$1"
  local address="$2"
  local mode="$3"
  local payload_variant="${ARTIFACT_DIR}/oem-uefi-probes/${probe_name}.elf"
  local shim_prefix="${ARTIFACT_DIR}/oem-uefi-probes/${probe_name}"

  python3 "${ROOT_DIR}/scripts/patch_elf_address_probe.py" \
    --input "${PAYLOAD}" \
    --address "${address}" \
    --mode "${mode}" \
    --output "${payload_variant}"

  python3 "${ROOT_DIR}/scripts/build_probe_kernel_shim.py" \
    --payload-elf "${payload_variant}" \
    --mode jump-payload \
    --output-prefix "${shim_prefix}"

  python3 "${ROOT_DIR}/scripts/repack_8e_style_boot.py" \
    --template-8e "${TEMPLATE_BOOT}" \
    --kernel-gzip "${shim_prefix}.raw.bin.gz" \
    --output "${shim_prefix}.boot.img"

  python3 "${ROOT_DIR}/scripts/analyze_kernel_shim_layout.py" \
    --input "${shim_prefix}.boot.img" \
    --output-dir "${ARTIFACT_DIR}/oem"

  rm -f \
    "${ARTIFACT_DIR}/oem/${probe_name}.boot.kernel.bin" \
    "${ARTIFACT_DIR}/oem/${probe_name}.boot.kernel.unpacked.bin"
}

build_one "pineapple-oem-uefi-code-entry-reset"        "0xa700e768" "reset"
build_one "pineapple-oem-uefi-systab-ok-reset"         "0xa700e7b4" "reset"
build_one "pineapple-oem-uefi-before-dispatch-reset"   "0xa700e810" "reset"
build_one "pineapple-oem-uefi-after-dispatch-reset"    "0xa700e814" "reset"
