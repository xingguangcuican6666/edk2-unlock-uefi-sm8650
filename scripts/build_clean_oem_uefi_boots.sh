#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_DIR="${1:-${ROOT_DIR}/out/artifacts}"
ARTIFACT_DIR="$(python3 -c 'import pathlib,sys; print(pathlib.Path(sys.argv[1]).resolve())' "${ARTIFACT_DIR}")"
PAYLOAD="${ROOT_DIR}/imgs/uefi.elf"
TEMPLATE_BOOT="${ROOT_DIR}/imgs/8e.img"

mkdir -p "${ARTIFACT_DIR}/clean-oem-uefi" "${ARTIFACT_DIR}/oem"

build_boot_from_payload() {
  local payload_variant="$1"
  local output_name="$2"
  local shim_prefix="${ARTIFACT_DIR}/clean-oem-uefi/${output_name}"

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
    "${ARTIFACT_DIR}/oem/${output_name}.boot.kernel.bin" \
    "${ARTIFACT_DIR}/oem/${output_name}.boot.kernel.unpacked.bin"
}

# Truly clean executable OEM UEFI boot: no probe, no forced reset, no loop patch.
build_boot_from_payload "${PAYLOAD}" "pineapple-oem-uefi-clean"

# Minimal functional variant: only bypass InstallConfigurationTable(), no reset probe.
skip_installcfg_payload="${ARTIFACT_DIR}/clean-oem-uefi/pineapple-oem-uefi-clean-skip-installcfg.elf"
python3 "${ROOT_DIR}/scripts/patch_elf_address_probe.py" \
  --input "${PAYLOAD}" \
  --address "0xa700e810" \
  --mode success \
  --output "${skip_installcfg_payload}"
build_boot_from_payload "${skip_installcfg_payload}" "pineapple-oem-uefi-clean-skip-installcfg"

cat > "${ARTIFACT_DIR}/clean-oem-uefi/README.md" <<'EOF'
`clean-oem-uefi/` contains non-probe executable OEM UEFI boot images.

Files:
- `pineapple-oem-uefi-clean.boot.img`
  unmodified `imgs/uefi.elf`, booted through the executable shim with no reset/loop probe patch

- `pineapple-oem-uefi-clean-skip-installcfg.boot.img`
  same as above, except `0xa700e810` is patched from `blr x3` to `mov x0, #0`
  this bypasses the problematic `InstallConfigurationTable()` call without adding any reset probe

These are the images to use as actual control / complete candidates.
Do not use the older `pineapple-final-boot.img` / `pineapple-uefi-fv-8e-style-boot.img` as clean controls:
those older images were built from the deprecated non-executable shim path.
EOF
