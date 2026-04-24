This directory contains a conservative extraction/repack experiment:

- OEM 8e UEFI/DXE/runtime are preserved from `imgs/8e.zip`
- `LinuxLoader` is replaced with the locally built 8gen3/open-source LinuxLoader FFS
- `DualStageLoader` is injected as an extra FFS from the local build
- The final image is repacked back into an 8e-style Android `boot.img`

Primary artifact:
- `pineapple-8e-oem-linuxloader.boot.img`
