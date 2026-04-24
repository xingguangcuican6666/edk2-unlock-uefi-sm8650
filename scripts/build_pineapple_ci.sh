#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${OUT_DIR:-${ROOT_DIR}/out}"
ARTIFACT_DIR="${ARTIFACT_DIR:-${OUT_DIR}/artifacts}"
BUILD_OUTPUT_DIR="${BUILD_OUTPUT_DIR:-${OUT_DIR}/Build}"
BOARD_NAME="${BOARD_NAME:-pineapple}"
BOOT_HEADER_VERSION="${BOOT_HEADER_VERSION:-4}"
BOOT_CMDLINE="${BOOT_CMDLINE:-}"
BUILD_TARGET="${BUILD_TARGET:-DEBUG}"
TOOL_CHAIN_TAG="${TOOL_CHAIN_TAG:-CLANG35}"
TARGET_ARCH="${TARGET_ARCH:-AARCH64}"
LOG_FILE="${LOG_FILE:-${OUT_DIR}/build_modulepkg.log}"

mkdir -p "${ARTIFACT_DIR}"
mkdir -p "${OUT_DIR}"

pushd "${ROOT_DIR}" >/dev/null

export WORKSPACE="${ROOT_DIR}"
export PACKAGES_PATH="${ROOT_DIR}"
export BUILD_NATIVE_AARCH64=true
export CLANG35_BIN="${CLANG35_BIN:-/usr/bin/}"
export FUSE_LD="${FUSE_LD:-/usr/bin/ld.lld}"
export MAKEPATH="${MAKEPATH:-/usr/bin/}"
export ABL_SRC="."

CLANG_MAJOR="$("${CLANG35_BIN}clang" --version | sed -n 's/.*clang version \([0-9][0-9]*\).*/\1/p' | head -n1)"
if [[ -n "${CLANG_MAJOR}" && "${CLANG_MAJOR}" -ge 17 ]]; then
  export CLANG_EXTRA_DLINK_FLAGS="-Wl,--no-relax -Wl,--apply-dynamic-relocs"
fi

. "${ROOT_DIR}/edksetup.sh"
make -C BaseTools

. "${ROOT_DIR}/QcomModulePkg/build.config.msm.${BOARD_NAME}"

build \
  -p "${ROOT_DIR}/QcomModulePkg/QcomModulePkg.dsc" \
  -a "${TARGET_ARCH}" \
  -t "${TOOL_CHAIN_TAG}" \
  -b "${BUILD_TARGET}" \
  -j "${LOG_FILE}" \
  -D ABL_OUT_DIR="${BUILD_OUTPUT_DIR}" \
  -D BOARD_BOOTLOADER_PRODUCT_NAME="${BOARD_NAME}" \
  -D VERIFIED_BOOT_LE="${VERIFIED_BOOT_LE}" \
  -D VERIFIED_BOOT_ENABLED="${VERIFIED_BOOT_ENABLED}" \
  -D AB_RETRYCOUNT_DISABLE="${AB_RETRYCOUNT_DISABLE}" \
  -D TARGET_BOARD_TYPE_AUTO="${TARGET_BOARD_TYPE_AUTO}" \
  -D BUILD_USES_RECOVERY_AS_BOOT="${BUILD_USES_RECOVERY_AS_BOOT}" \
  -D DISABLE_PARALLEL_DOWNLOAD_FLASH="${DISABLE_PARALLEL_DOWNLOAD_FLASH}" \
  -D REMOVE_CARVEOUT_REGION="${REMOVE_CARVEOUT_REGION}" \
  -D PVMFW_BCC_ENABLED="${PVMFW_BCC_ENABLED}"

BUILD_ROOT="${BUILD_OUTPUT_DIR}/${BUILD_TARGET}_${TOOL_CHAIN_TAG}"
FV_IMAGE="${BUILD_ROOT}/FV/FVMAIN_COMPACT.Fv"
LINUX_LOADER_EFI="${BUILD_ROOT}/${TARGET_ARCH}/QcomModulePkg/Application/LinuxLoader/LinuxLoader/${BUILD_TARGET}/LinuxLoader.efi"
DUAL_STAGE_LOADER_EFI="${BUILD_ROOT}/${TARGET_ARCH}/QcomModulePkg/Application/DualStageLoader/DualStageLoader/${BUILD_TARGET}/DualStageLoader.efi"
UNSIGNED_ABL="${OUT_DIR}/unsigned_abl.elf"

python3 "${ROOT_DIR}/QcomModulePkg/Tools/image_header.py" \
  "${FV_IMAGE}" \
  "${UNSIGNED_ABL}" \
  0x9FA00000 \
  elf \
  32 \
  nohash

popd >/dev/null

python3 "${ROOT_DIR}/scripts/pack_bootimg.py" \
  --kernel "${UNSIGNED_ABL}" \
  --output "${ARTIFACT_DIR}/pineapple-dualstage-boot.img" \
  --header-version "${BOOT_HEADER_VERSION}" \
  --cmdline "${BOOT_CMDLINE}"

cp "${UNSIGNED_ABL}" "${ARTIFACT_DIR}/pineapple-unsigned_abl.elf"
cp "${LINUX_LOADER_EFI}" "${ARTIFACT_DIR}/pineapple-stage1-linuxloader.efi"
cp "${DUAL_STAGE_LOADER_EFI}" "${ARTIFACT_DIR}/pineapple-stage2-loader.efi"

if [[ -f "${LOG_FILE}" ]]; then
  cp "${LOG_FILE}" "${ARTIFACT_DIR}/build_modulepkg.log"
fi

cat > "${ARTIFACT_DIR}/manifest.txt" <<EOF
target=${BOARD_NAME}
tool_chain_tag=${TOOL_CHAIN_TAG}
target_arch=${TARGET_ARCH}
build_target=${BUILD_TARGET}
boot_header_version=${BOOT_HEADER_VERSION}
boot_cmdline=${BOOT_CMDLINE}
boot_img=pineapple-dualstage-boot.img
primary_uefi=pineapple-stage1-linuxloader.efi
embedded_stage2_efi=pineapple-stage2-loader.efi
unsigned_abl=pineapple-unsigned_abl.elf
EOF
