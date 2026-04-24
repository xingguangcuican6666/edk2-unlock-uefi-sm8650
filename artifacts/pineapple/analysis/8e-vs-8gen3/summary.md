# 8e vs 8gen3 UEFI Dump Comparison

- Reference: `imgs/8e.zip`
- Candidate: `imgs/8gen3.zip`
- Reference entries: `3544`
- Candidate entries: `118`
- Reference modules: `225`
- Candidate modules: `2`

## Key Gap

- Modules present only in reference: `224`
- Modules present only in candidate: `1`
- Shared modules: `1`

## Largest Reference-Only Modules

- `EdkShell`: 985106 bytes, sections: PE32 image section, UI section
- `VariableRuntimeDxe`: 967242 bytes, sections: DXE dependency section, PE32 image section, UI section
- `BaseCryptoDriverDxeAARCH64`: 901210 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `ufpdevicefw`: 443936 bytes, sections: PE32 image section, UI section, Version section
- `RegularExpressionDxe`: 424490 bytes, sections: PE32 image section, UI section
- `Low Battery`: 403690 bytes, sections: Raw section, UI section
- `Too Hot`: 403682 bytes, sections: Raw section, UI section
- `BdsDxe`: 395880 bytes, sections: DXE dependency section, PE32 image section, UI section
- `Logo`: 390002 bytes, sections: Raw section, UI section
- `MsBootPolicy`: 356890 bytes, sections: PE32 image section, UI section
- `DALSys`: 344104 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `BootMenu`: 343726 bytes, sections: DXE dependency section, PE32 image section, Raw section, UI section
- `ClockDxe`: 278608 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `PmicDxe`: 274510 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `DxeCore`: 204304 bytes, sections: PE32 image section, UI section
- `UCDxe`: 192550 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `ICBDxe`: 188492 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `SettingsManagerDxe`: 165594 bytes, sections: DXE dependency section, PE32 image section, UI section
- `FrontPage`: 161860 bytes, sections: PE32 image section, Raw section, UI section
- `UFSDxe`: 151576 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `HALIOMMU`: 143602 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `IdentityAndAuthManagerDxe`: 130828 bytes, sections: DXE dependency section, PE32 image section, UI section
- `SdccDxe`: 127036 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `HiiDatabase`: 123434 bytes, sections: DXE dependency section, PE32 image section, UI section
- `PILDxe`: 118824 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `SetupBrowser`: 108642 bytes, sections: DXE dependency section, PE32 image section, UI section
- `OnScreenKeyboardDxe`: 100114 bytes, sections: DXE dependency section, PE32 image section, UI section
- `UsbfnDwc3Dxe`: 98608 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `DfciMenu`: 94984 bytes, sections: DXE dependency section, PE32 image section, Raw section, UI section
- `UsbConfigDxe`: 90236 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `XhciDxe`: 90136 bytes, sections: PE32 image section, UI section, Version section
- `PciBusDxe`: 80126 bytes, sections: DXE dependency section, PE32 image section, UI section
- `DisplayEngine`: 78514 bytes, sections: DXE dependency section, PE32 image section, Raw section, UI section
- `EnvDxeEnhanced`: 77864 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `UsbBusDxe`: 77852 bytes, sections: PE32 image section, UI section, Version section
- `ULogDxe`: 77850 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `NpaDxe`: 77848 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `MsUiThemeProtocol`: 75318 bytes, sections: DXE dependency section, PE32 image section, UI section
- `VcsDxe`: 69708 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `TzDxeLA`: 69656 bytes, sections: PE32 image section, UI section, Version section
- `SimpleWindowManagerDxe`: 61744 bytes, sections: DXE dependency section, PE32 image section, Raw section, UI section
- `I2C`: 61528 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `SPI`: 61528 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `RpmhDxe`: 61500 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `VerifiedBootDxe`: 61480 bytes, sections: PE32 image section, UI section, Version section
- `UsbMsdDxe`: 61470 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `DevicePathDxe`: 60974 bytes, sections: DXE dependency section, PE32 image section, UI section
- `AcpiTables`: 58142 bytes, sections: Raw section, UI section
- `NvmExpressDxe`: 57884 bytes, sections: PE32 image section, UI section
- `RmVmDxe`: 57370 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `FvDxe`: 57366 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `SecurityStubDxe`: 54340 bytes, sections: DXE dependency section, PE32 image section, UI section
- `UsbDeviceDxe`: 53300 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `ScmDxeCompat`: 53282 bytes, sections: PE32 image section, UI section, Version section
- `PartitionDxe`: 53274 bytes, sections: PE32 image section, UI section
- `ScsiDisk`: 52754 bytes, sections: PE32 image section, UI section
- `Fat`: 51720 bytes, sections: PE32 image section, UI section
- `AdvancedFileLogger`: 50762 bytes, sections: DXE dependency section, PE32 image section, UI section
- `DriverHealthManagerDxe`: 50384 bytes, sections: DXE dependency section, PE32 image section, Raw section, UI section
- `ChipInfo`: 49214 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `UsbMassStorageDxe`: 49196 bytes, sections: PE32 image section, UI section, Version section
- `DALTLMM`: 49194 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `PmicGlinkDxe`: 49188 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `logo1.bmp`: 48446 bytes, sections: Raw section, UI section
- `ArmCpuDxe`: 47142 bytes, sections: DXE dependency section, PE32 image section, UI section
- `MailboxDxe`: 45104 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `ResetRuntimeDxe`: 45098 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `DDRInfoDxe`: 45088 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `SPMI`: 45076 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `TerminalDxe`: 44056 bytes, sections: PE32 image section, UI section
- `XhciPciEmulation`: 41236 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `ButtonsDxe`: 41098 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `AcpiTableDxe`: 41004 bytes, sections: DXE dependency section, PE32 image section, UI section
- `UsbPwrCtrlDxe`: 40998 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `PILProxyDxe`: 40994 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `ASN1X509Dxe`: 40992 bytes, sections: PE32 image section, UI section, Version section
- `ConSplitterDxe`: 40990 bytes, sections: PE32 image section, UI section
- `SecRSADxe`: 40990 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `SmemDxe`: 40986 bytes, sections: DXE dependency section, PE32 image section, UI section, Version section
- `DiskIoDxe`: 40980 bytes, sections: PE32 image section, UI section

## Candidate-Only Modules

- `DualStageLoader`

## Top-Level Layout

- Reference top levels: `[('kernel.dump', 2654), ('kernel', 1), ('kernel.guids.csv', 1), ('kernel.report.txt', 1)]`
- Candidate top levels: `[('kernel.dump', 84), ('kernel', 1), ('kernel.guids.csv', 1), ('kernel.report.txt', 1)]`

## Extension Mix

- Reference extensions: `[('.bin', 1768), ('.txt', 887), ('<none>', 1), ('.csv', 1)]`
- Candidate extensions: `[('.bin', 54), ('.txt', 31), ('<none>', 1), ('.csv', 1)]`

