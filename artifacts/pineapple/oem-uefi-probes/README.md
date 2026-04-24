`oem-uefi-probes/` contains executable probes on the real OEM `imgs/uefi.elf` path.

These images do not use the deprecated fake `uefi-probes/` LinuxLoader wrapper path.
They patch specific AArch64 addresses inside `uefi.elf`, then boot that patched OEM payload
through the known-good executable shim.

Test order:
- `pineapple-oem-uefi-code-entry-reset.boot.img`
  address: `0xa700e768`
  meaning: first real AArch64 code reached after the Linux-image style entry branch

- `pineapple-oem-uefi-systab-ok-reset.boot.img`
  address: `0xa700e7b4`
  meaning: reached after the EFI system table signature check passes

- `pineapple-oem-uefi-before-dispatch-reset.boot.img`
  address: `0xa700e810`
  meaning: reached immediately before the indirect dispatch call into deeper UEFI logic

- `pineapple-oem-uefi-after-dispatch-reset.boot.img`
  address: `0xa700e814`
  meaning: reached only if that indirect dispatch call returns

Interpretation:
- if `code-entry-reset` does not reboot, the problem is before real OEM UEFI code starts
- if `code-entry-reset` reboots but `systab-ok-reset` does not, the problem is in the early wrapper / system table validation path
- if `before-dispatch-reset` reboots but `after-dispatch-reset` does not, control enters deeper OEM UEFI logic and hangs before returning
