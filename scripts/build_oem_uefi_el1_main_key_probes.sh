#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_DIR="${1:-${ROOT_DIR}/out/artifacts}"
ARTIFACT_DIR="$(python3 -c 'import pathlib,sys; print(pathlib.Path(sys.argv[1]).resolve())' "${ARTIFACT_DIR}")"
PAYLOAD="${ROOT_DIR}/imgs/uefi.elf"
TEMPLATE_BOOT="${ROOT_DIR}/imgs/8e.img"

mkdir -p "${ARTIFACT_DIR}/oem-uefi-probes" "${ARTIFACT_DIR}/oem"

build_boot_from_payload() {
  local payload_variant="$1"
  local probe_name="$2"
  local shim_prefix="${ARTIFACT_DIR}/oem-uefi-probes/${probe_name}"

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

base_payload="${ARTIFACT_DIR}/oem-uefi-probes/pineapple-oem-uefi-skip-installcfg-base.elf"
python3 "${ROOT_DIR}/scripts/patch_elf_address_probe.py" \
  --input "${PAYLOAD}" \
  --address "0xa700e810" \
  --mode success \
  --output "${base_payload}"

build_one() {
  local probe_name="$1"
  local address="$2"
  local payload_variant="${ARTIFACT_DIR}/oem-uefi-probes/${probe_name}.elf"

  python3 "${ROOT_DIR}/scripts/patch_elf_address_probe.py" \
    --input "${base_payload}" \
    --address "${address}" \
    --mode reset \
    --output "${payload_variant}"

  build_boot_from_payload "${payload_variant}" "${probe_name}"
}

# EL1 main-path coarse key points: early, middle, late.
build_one "pineapple-oem-uefi-el1-early-reset"  "0xa700f0c0"
build_one "pineapple-oem-uefi-el1-mid-reset"    "0xa700f140"
build_one "pineapple-oem-uefi-el1-late-reset"   "0xa700f590"
