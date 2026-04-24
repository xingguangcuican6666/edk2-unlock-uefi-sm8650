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


Post-InstallConfigurationTable() chain probes:
- `pineapple-oem-uefi-after-call1-reset.boot.img`
  reset at `0xa700e8ac`, so the first post-bypass helper call returned

- `pineapple-oem-uefi-after-call2-reset.boot.img`
  reset at `0xa700e8b0`, so the second helper call returned

- `pineapple-oem-uefi-after-call3-reset.boot.img`
  reset at `0xa700e8b4`, so the third helper call returned

- `pineapple-oem-uefi-after-call4-reset.boot.img`
  reset at `0xa700e8b8`, so all four helper calls returned and execution reached the `CurrentEL` check


CurrentEL and post-transition probes:
- `pineapple-oem-uefi-el3-setup-reset.boot.img`
  reset at `0xa700e8d8`, so the EL3 setup branch was taken

- `pineapple-oem-uefi-post-el-transition-reset.boot.img`
  reset at `0xa700e994`, so execution reached the common post-transition path

- `pineapple-oem-uefi-el2-path-reset.boot.img`
  reset at `0xa700e9bc`, so the optional EL2-specific path was taken

- `pineapple-oem-uefi-after-el-check-reset.boot.img`
  reset at `0xa700e9c4`, so execution passed the second CurrentEL check and its optional branch


Post-EL main path probes:
- `pineapple-oem-uefi-after-common-call-reset.boot.img`
  reset at `0xa700e9cc`, so the common call at `0xa700e9c8` returned

- `pineapple-oem-uefi-primary-core-reset.boot.img`
  reset at `0xa700e9f0`, so execution entered the primary-core path after the MPIDR branch

- `pineapple-oem-uefi-after-stack-fill-reset.boot.img`
  reset at `0xa700ea10`, so the stack/pattern fill loop completed

- `pineapple-oem-uefi-after-alloc-reset.boot.img`
  reset at `0xa700ea5c`, so the allocation call at `0xa700ea58` returned

- `pineapple-oem-uefi-before-final-call-reset.boot.img`
  reset at `0xa700ea7c`, so execution reached the final handoff call in this block


Final call probes around `bl 0xa7010940`:
- `pineapple-oem-uefi-final-call-entry-reset.boot.img`
  reset at `0xa7010940`, proving control entered the final call target

- `pineapple-oem-uefi-final-call-after-subcall1.boot.img`
  reset at `0xa7010958`, proving the first inner subcall returned

- `pineapple-oem-uefi-final-call-after-subcall2.boot.img`
  reset at `0xa701095c`, proving the second inner subcall returned

- `pineapple-oem-uefi-final-call-after-subcall3.boot.img`
  reset at `0xa7010968`, proving the third inner subcall returned and the function is about to return

- `pineapple-oem-uefi-final-call-return-reset.boot.img`
  reset at `0xa700ea80`, proving the whole final call returned back to its caller


Coarse EL1 probes:
- `pineapple-oem-uefi-el1-early-reset.boot.img`
  reset at `0xa700f0c0`, after the early EL1 init block

- `pineapple-oem-uefi-el1-mid-reset.boot.img`
  reset at `0xa700f140`, after the mid-path context/build stage

- `pineapple-oem-uefi-el1-late-reset.boot.img`
  reset at `0xa700f590`, a much later stage in the EL1 main path, closer to the deeper load/handoff chain
