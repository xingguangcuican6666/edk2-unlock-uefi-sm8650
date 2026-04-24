#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${OUT_DIR:-${ROOT_DIR}/out}"
ARTIFACT_DIR="${ARTIFACT_DIR:-${OUT_DIR}/artifacts}"
OEM_ARTIFACT_DIR="${ARTIFACT_DIR}/oem"
ANALYSIS_DIR="${ARTIFACT_DIR}/analysis/8e-vs-8gen3"
BUILD_OUTPUT_DIR="${BUILD_OUTPUT_DIR:-${OUT_DIR}/Build}"
BOARD_NAME="${BOARD_NAME:-pineapple}"
BOOT_HEADER_VERSION="${BOOT_HEADER_VERSION:-4}"
BOOT_CMDLINE="${BOOT_CMDLINE:-}"
BUILD_TARGET="${BUILD_TARGET:-DEBUG}"
TOOL_CHAIN_TAG="${TOOL_CHAIN_TAG:-CLANG35}"
TARGET_ARCH="${TARGET_ARCH:-AARCH64}"
LOG_FILE="${LOG_FILE:-${OUT_DIR}/build_modulepkg.log}"
TARGET_BUILD_VARIANT="${TARGET_BUILD_VARIANT:-userdebug}"

mkdir -p "${ARTIFACT_DIR}"
mkdir -p "${OEM_ARTIFACT_DIR}"
mkdir -p "${ANALYSIS_DIR}"
mkdir -p "${OUT_DIR}"

pushd "${ROOT_DIR}" >/dev/null

export WORKSPACE="${ROOT_DIR}"
export PACKAGES_PATH="${ROOT_DIR}"
export CONF_PATH="${ROOT_DIR}/Conf"
export ROOT_DIR="${WORKSPACE}"
export BUILD_NATIVE_AARCH64=true
export TARGET_BUILD_VARIANT
export CLANG35_BIN="${CLANG35_BIN:-/usr/bin/}"
export FUSE_LD="${FUSE_LD:-/usr/bin/ld.lld}"
export MAKEPATH="${MAKEPATH:-/usr/bin/}"
export ABL_SRC="."

mkdir -p "${CONF_PATH}"

CLANG_MAJOR="$("${CLANG35_BIN}clang" --version | sed -n 's/.*clang version \([0-9][0-9]*\).*/\1/p' | head -n1)"
if [[ -n "${CLANG_MAJOR}" && "${CLANG_MAJOR}" -ge 17 ]]; then
  export CLANG_EXTRA_DLINK_FLAGS="-Wl,--no-relax -Wl,--apply-dynamic-relocs"
fi

set +u
. "${ROOT_DIR}/edksetup.sh"
set -u
make -C BaseTools

set +u
. "${ROOT_DIR}/QcomModulePkg/build.config.msm.${BOARD_NAME}"
set -u

INIT_BIN_VALUE="${INIT_BIN_LE:-/init}"
TARGET_AUDIO_FRAMEWORK_VALUE="${TARGET_AUDIO_FRAMEWORK:-unknown}"
BASE_ADDRESS_VALUE="${BASE_ADDRESS:-0x80000000}"

build_args=(
  build
  -p "${ROOT_DIR}/QcomModulePkg/QcomModulePkg.dsc"
  -a "${TARGET_ARCH}"
  -t "${TOOL_CHAIN_TAG}"
  -b "${BUILD_TARGET}"
  -j "${LOG_FILE}"
  -D "ABL_OUT_DIR=${BUILD_OUTPUT_DIR}"
  -D "BOARD_BOOTLOADER_PRODUCT_NAME=${BOARD_NAME}"
)

append_define() {
  local name="$1"
  local value="${2-}"
  if [[ -n "${value}" ]]; then
    build_args+=(-D "${name}=${value}")
  fi
}

append_define VERIFIED_BOOT_LE "${VERIFIED_BOOT_LE-}"
append_define VERIFIED_BOOT_ENABLED "${VERIFIED_BOOT_ENABLED-}"
append_define ROOT_PARTLABEL_SUPPORT "${ROOT_PARTLABEL_SUPPORT-}"
append_define USE_DUMMY_BCC "${USE_DUMMY_BCC-}"
append_define EARLY_ETH_ENABLED "${EARLY_ETH_ENABLED-}"
append_define AUTO_LVGVM_ABL "${AUTO_LVGVM_ABL-}"
append_define HIBERNATION_SUPPORT_NO_AES "${HIBERNATION_SUPPORT_NO_AES-}"
append_define HIBERNATION_SUPPORT_AES "${HIBERNATION_SUPPORT_AES-}"
append_define HIBERNATION_TZ_ENCRYPTION "${HIBERNATION_TZ_ENCRYPTION-}"
append_define APPEND_RAM_PARTITIONS_TO_MEM_NODE "${APPEND_RAM_PARTITIONS_TO_MEM_NODE-}"
append_define EARLY_ETH_AS_DLKM "${EARLY_ETH_AS_DLKM-}"
append_define BOOTIMAGE_LOAD_VERIFY_IN_PARALLEL "${BOOTIMAGE_LOAD_VERIFY_IN_PARALLEL-}"
append_define AB_RETRYCOUNT_DISABLE "${AB_RETRYCOUNT_DISABLE-}"
append_define TARGET_BOARD_TYPE_AUTO "${TARGET_BOARD_TYPE_AUTO-}"
append_define VERITY_LE "${VERITY_LE-}"
append_define USER_BUILD_VARIANT "${USER_BUILD_VARIANT-}"
append_define DISABLE_PARALLEL_DOWNLOAD_FLASH "${DISABLE_PARALLEL_DOWNLOAD_FLASH-}"
append_define ENABLE_LE_VARIANT "${ENABLE_LE_VARIANT-}"
append_define WEAR_OS "${WEAR_OS-}"
append_define ENABLE_LV_ATOMIC_AB "${ENABLE_LV_ATOMIC_AB-}"
append_define BUILD_USES_RECOVERY_AS_BOOT "${BUILD_USES_RECOVERY_AS_BOOT-}"
append_define INIT_BIN "${INIT_BIN_VALUE}"
append_define UBSAN_UEFI_GCC_FLAG_UNDEFINED "${UBSAN_GCC_FLAG_UNDEFINED-}"
append_define UBSAN_UEFI_GCC_FLAG_ALIGNMENT "${UBSAN_GCC_FLAG_ALIGNMENT-}"
append_define NAND_SQUASHFS_SUPPORT "${NAND_SQUASHFS_SUPPORT-}"
append_define BASE_ADDRESS "${BASE_ADDRESS_VALUE}"
append_define LINUX_BOOT_CPU_SELECTION_ENABLED "${LINUX_BOOT_CPU_SELECTION_ENABLED-}"
append_define TARGET_LINUX_BOOT_CPU_ID "${TARGET_LINUX_BOOT_CPU_ID-}"
append_define HIBERNATION_SWAP_PARTITION_NAME "${HIBERNATION_SWAP_PARTITION_NAME-}"
append_define DISABLE_DTBO_PARTITION "${DISABLE_DTBO_PARTITION-}"
append_define SUPPORT_AB_BOOT_LXC "${SUPPORT_AB_BOOT_LXC-}"
append_define TARGET_SUPPORTS_EARLY_USB_INIT "${TARGET_SUPPORTS_EARLY_USB_INIT-}"
append_define TARGET_AUDIO_FRAEMEWORK "${TARGET_AUDIO_FRAMEWORK_VALUE}"
append_define ENABLE_EARLY_SERVICES "${ENABLE_EARLY_SERVICES-}"
append_define KERNEL_LOAD_ADDRESS "${KERNEL_LOAD_ADDRESS-}"
append_define KERNEL_SIZE_RESERVED "${KERNEL_SIZE_RESERVED-}"
append_define DISABLE_KERNEL_PROTOCOL "${DISABLE_KERNEL_PROTOCOL-}"
append_define NAND_UBI_VOLUME_FLASHING_ENABLED "${NAND_UBI_VOLUME_FLASHING_ENABLED-}"
append_define REMOVE_CARVEOUT_REGION "${REMOVE_CARVEOUT_REGION-}"
append_define QSPA_BOOTCONFIG_ENABLE "${QSPA_BOOTCONFIG_ENABLE-}"
append_define AUTO_VIRT_ABL "${AUTO_VIRT_ABL-}"
append_define DDR_SUPPORTS_SCT_CONFIG "${DDR_SUPPORTS_SCT_CONFIG-}"

"${build_args[@]}"

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

if [[ -f "${ROOT_DIR}/imgs/boot.img" ]]; then
  python3 "${ROOT_DIR}/scripts/repack_stock_boot.py" \
    --template-boot "${ROOT_DIR}/imgs/boot.img" \
    --signature-blob "${UNSIGNED_ABL}" \
    --output "${ARTIFACT_DIR}/pineapple-dualstage-boot.img"
else
  python3 "${ROOT_DIR}/scripts/pack_bootimg.py" \
    --kernel "${UNSIGNED_ABL}" \
    --output "${ARTIFACT_DIR}/pineapple-dualstage-boot.img" \
    --header-version "${BOOT_HEADER_VERSION}" \
    --cmdline "${BOOT_CMDLINE}"
fi

if [[ -f "${ROOT_DIR}/imgs/abl.elf" ]]; then
  python3 "${ROOT_DIR}/scripts/analyze_qcom_elf_fv.py" \
    --input "${ROOT_DIR}/imgs/abl.elf" \
    --output-dir "${OEM_ARTIFACT_DIR}"
fi

if [[ -f "${ROOT_DIR}/imgs/uefi.elf" ]]; then
  python3 "${ROOT_DIR}/scripts/analyze_qcom_elf_fv.py" \
    --input "${ROOT_DIR}/imgs/uefi.elf" \
    --output-dir "${OEM_ARTIFACT_DIR}"
fi

if [[ -f "${ROOT_DIR}/imgs/8e.img" ]]; then
  python3 "${ROOT_DIR}/scripts/build_kernel_shim_payload.py" \
    --fv "${FV_IMAGE}" \
    --stage1-efi "${LINUX_LOADER_EFI}" \
    --stage2-efi "${DUAL_STAGE_LOADER_EFI}" \
    --unsigned-abl "${UNSIGNED_ABL}" \
    --output-prefix "${ARTIFACT_DIR}/pineapple-8e-style-shim"

  python3 "${ROOT_DIR}/scripts/repack_8e_style_boot.py" \
    --template-8e "${ROOT_DIR}/imgs/8e.img" \
    --kernel-gzip "${ARTIFACT_DIR}/pineapple-8e-style-shim.raw.bin.gz" \
    --output "${ARTIFACT_DIR}/pineapple-8e-style-boot.img"

  python3 "${ROOT_DIR}/scripts/analyze_kernel_shim_layout.py" \
    --input "${ARTIFACT_DIR}/pineapple-8e-style-boot.img" \
    --output-dir "${OEM_ARTIFACT_DIR}"

  python3 "${ROOT_DIR}/scripts/analyze_kernel_shim_layout.py" \
    --input "${ROOT_DIR}/imgs/8e.img" \
    --output-dir "${OEM_ARTIFACT_DIR}"
  rm -f \
    "${OEM_ARTIFACT_DIR}/8e.kernel.bin" \
    "${OEM_ARTIFACT_DIR}/8e.kernel.unpacked.bin" \
    "${OEM_ARTIFACT_DIR}/pineapple-8e-style-boot.kernel.bin" \
    "${OEM_ARTIFACT_DIR}/pineapple-8e-style-boot.kernel.unpacked.bin"
fi

if [[ -f "${ROOT_DIR}/imgs/8e.zip" && -f "${ROOT_DIR}/imgs/8gen3.zip" ]]; then
  python3 "${ROOT_DIR}/scripts/compare_uefi_dumps.py" \
    --reference "${ROOT_DIR}/imgs/8e.zip" \
    --candidate "${ROOT_DIR}/imgs/8gen3.zip" \
    --output-dir "${ANALYSIS_DIR}"
fi

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
boot_template=imgs/boot.img
init_boot_template=imgs/init_boot.img
primary_uefi=pineapple-stage1-linuxloader.efi
embedded_stage2_efi=pineapple-stage2-loader.efi
unsigned_abl=pineapple-unsigned_abl.elf
oem_manifests_dir=oem
analysis_dir=analysis/8e-vs-8gen3
eight_e_style_boot_img=pineapple-8e-style-boot.img
eight_e_style_shim=pineapple-8e-style-shim.raw.bin.gz
EOF
