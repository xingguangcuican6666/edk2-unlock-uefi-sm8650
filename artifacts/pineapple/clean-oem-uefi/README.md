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
