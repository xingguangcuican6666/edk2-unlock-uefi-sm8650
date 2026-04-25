#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${OUT_DIR:-${ROOT_DIR}/out/unlock-poweroff}"
ARTIFACT_DIR="${ARTIFACT_DIR:-${OUT_DIR}/artifacts}"

mkdir -p "${ARTIFACT_DIR}"

FORCE_EL1_UNLOCK_AND_SHUTDOWN=1 \
OUT_DIR="${OUT_DIR}" \
ARTIFACT_DIR="${ARTIFACT_DIR}" \
"${ROOT_DIR}/scripts/build_pineapple_ci.sh"

cp -f "${ARTIFACT_DIR}/pineapple-dualstage-boot.img" \
  "${ARTIFACT_DIR}/pineapple-dualstage-unlock-poweroff-boot.img"

if [[ -f "${ARTIFACT_DIR}/pineapple-stage1-linuxloader.efi" ]]; then
  cp -f "${ARTIFACT_DIR}/pineapple-stage1-linuxloader.efi" \
    "${ARTIFACT_DIR}/pineapple-stage1-linuxloader-unlock-poweroff.efi"
fi

if [[ -f "${ARTIFACT_DIR}/manifest.txt" ]]; then
  cat >> "${ARTIFACT_DIR}/manifest.txt" <<'EOF'
wrapped_artifact=pineapple-dualstage-unlock-poweroff-boot.img
wrapped_stage1_efi=pineapple-stage1-linuxloader-unlock-poweroff.efi
EOF
fi

cat > "${ARTIFACT_DIR}/unlock-poweroff.README.md" <<'EOF'
This variant enables FORCE_EL1_UNLOCK_AND_SHUTDOWN in the local LinuxLoader build.

Behavior when the image really reaches LinuxLoader after EL1:
- call SetDeviceUnlockValue(UNLOCK, TRUE)
- on success, call ShutdownDevice()

Primary artifact:
- pineapple-dualstage-unlock-poweroff-boot.img
EOF

echo "${ARTIFACT_DIR}/pineapple-dualstage-unlock-poweroff-boot.img"
