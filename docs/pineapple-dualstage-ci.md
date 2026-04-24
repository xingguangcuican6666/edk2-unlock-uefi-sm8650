# Pineapple Dual-Stage CI

This repository now builds a dual-stage `pineapple` boot payload in GitHub Actions.

Artifacts:

- `pineapple-dualstage-boot.img`: Android `boot.img` for the `boot` partition.
- `pineapple-unsigned_abl.elf`: standalone unsigned ABL artifact.
- `pineapple-stage1-linuxloader.efi`: primary UEFI payload embedded into the boot image.
- `pineapple-stage2-loader.efi`: EFI payload embedded into the primary UEFI firmware volume.

Boot flow:

1. The device boots `pineapple-dualstage-boot.img`.
2. The primary `LinuxLoader` UEFI starts.
3. On normal boot, `LinuxLoader` loads the embedded `DualStageLoader` EFI payload from the same firmware volume.
4. `DualStageLoader` performs the Android boot hand-off.
5. If stage 2 fails or returns, stage 1 falls back to the original direct boot path.

Notes:

- The workflow intentionally produces a bare `boot.img` for unlocked test devices.
- No AVB or `vbmeta` signing is applied.
- The boot image packer currently emits Android boot header v4 images only.
- Manual trigger paths:
  - GitHub UI: `workflow_dispatch`, with optional `target_ref`.
  - API: `repository_dispatch` event type `build-pineapple-dualstage`, for example with payload fields `ref` and `boot_cmdline`.
