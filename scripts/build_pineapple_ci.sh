#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${OUT_DIR:-${ROOT_DIR}/out}"
ARTIFACT_DIR="${ARTIFACT_DIR:-${OUT_DIR}/artifacts}"
BOOTLOADER_OUT="${BOOTLOADER_OUT:-${OUT_DIR}/obj/ABL_OBJ}"
BOARD_NAME="${BOARD_NAME:-pineapple}"
BOOT_HEADER_VERSION="${BOOT_HEADER_VERSION:-4}"
BOOT_CMDLINE="${BOOT_CMDLINE:-}"

mkdir -p "${ARTIFACT_DIR}"

make -C "${ROOT_DIR}" \
  BUILD_NATIVE_AARCH64=true \
  BOOTLOADER_OUT="${BOOTLOADER_OUT}" \
  BOARD_BOOTLOADER_PRODUCT_NAME="${BOARD_NAME}" \
  CLANG_BIN="${CLANG_BIN:-/usr/bin/}" \
  CLANG_PREFIX="${CLANG_PREFIX:-/usr/bin/aarch64-linux-gnu-}" \
  VERIFIED_BOOT_ENABLED=1 \
  VERIFIED_BOOT_LE=0 \
  AB_RETRYCOUNT_DISABLE=0 \
  TARGET_BOARD_TYPE_AUTO=0 \
  BUILD_USES_RECOVERY_AS_BOOT=0 \
  DISABLE_PARALLEL_DOWNLOAD_FLASH=0 \
  REMOVE_CARVEOUT_REGION=1 \
  PVMFW_BCC_ENABLED=-DPVMFW_BCC \
  all

python3 "${ROOT_DIR}/scripts/pack_bootimg.py" \
  --kernel "${OUT_DIR}/unsigned_abl.elf" \
  --output "${ARTIFACT_DIR}/pineapple-dualstage-boot.img" \
  --header-version "${BOOT_HEADER_VERSION}" \
  --cmdline "${BOOT_CMDLINE}"

cp "${OUT_DIR}/unsigned_abl.elf" "${ARTIFACT_DIR}/pineapple-unsigned_abl.elf"
cp "${OUT_DIR}/LinuxLoader.efi" "${ARTIFACT_DIR}/pineapple-stage1-linuxloader.efi"
cp "${OUT_DIR}/DualStageLoader.efi" "${ARTIFACT_DIR}/pineapple-stage2-loader.efi"

if [[ -f "${ROOT_DIR}/build_modulepkg.log" ]]; then
  cp "${ROOT_DIR}/build_modulepkg.log" "${ARTIFACT_DIR}/build_modulepkg.log"
fi

cat > "${ARTIFACT_DIR}/manifest.txt" <<EOF
target=${BOARD_NAME}
boot_header_version=${BOOT_HEADER_VERSION}
boot_cmdline=${BOOT_CMDLINE}
boot_img=pineapple-dualstage-boot.img
primary_uefi=pineapple-stage1-linuxloader.efi
embedded_stage2_efi=pineapple-stage2-loader.efi
unsigned_abl=pineapple-unsigned_abl.elf
EOF
