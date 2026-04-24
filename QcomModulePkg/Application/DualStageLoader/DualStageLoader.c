/*
 * Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "AutoGen.h"
#include "BootLinux.h"
#include "LinuxLoaderLib.h"
#include <FastbootLib/FastbootMain.h>
#include <Library/DeviceInfo.h>
#include <Library/HypervisorMvCalls.h>
#include <Library/PartitionTableUpdate.h>
#include <Library/StackCanary.h>
#include "Library/ThreadStack.h"
#include <Library/DrawUI.h>
#include <Protocol/GraphicsOutput.h>

#define DEFAULT_STACK_CHK_GUARD 0xc0c0c0c0

#ifndef PROBE_REBOOT_STAGE_ID
#define PROBE_REBOOT_STAGE_ID 0
#endif

BccParams_t BccParamsRecvdFromAVB = {{0}};
STATIC BOOLEAN BootIntoFastboot = FALSE;
STATIC BOOLEAN BootIntoRecovery = FALSE;
STATIC UINT32 BootDeviceType = EFI_MAX_FLASH_TYPE;

STATIC
VOID
GetStagePixel (
  IN  UINT32                          ColorId,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *Pixel
  )
{
  Pixel->Reserved = 0;
  switch (ColorId) {
    case BGR_WHITE:
      Pixel->Blue = 0xff; Pixel->Green = 0xff; Pixel->Red = 0xff; break;
    case BGR_BLACK:
      Pixel->Blue = 0x00; Pixel->Green = 0x00; Pixel->Red = 0x00; break;
    case BGR_ORANGE:
      Pixel->Blue = 0x00; Pixel->Green = 0xa5; Pixel->Red = 0xff; break;
    case BGR_YELLOW:
      Pixel->Blue = 0x00; Pixel->Green = 0xff; Pixel->Red = 0xff; break;
    case BGR_RED:
      Pixel->Blue = 0x00; Pixel->Green = 0x00; Pixel->Red = 0x98; break;
    case BGR_GREEN:
      Pixel->Blue = 0x00; Pixel->Green = 0xff; Pixel->Red = 0x00; break;
    case BGR_BLUE:
      Pixel->Blue = 0xff; Pixel->Green = 0x00; Pixel->Red = 0x00; break;
    case BGR_CYAN:
      Pixel->Blue = 0xff; Pixel->Green = 0xff; Pixel->Red = 0x00; break;
    case BGR_SILVER:
    default:
      Pixel->Blue = 0xc0; Pixel->Green = 0xc0; Pixel->Red = 0xc0; break;
  }
}

STATIC
VOID
RenderStageBanner (
  IN CONST CHAR8  *StageLabel,
  IN UINT32       BgColor,
  IN UINT32       AccentColor,
  IN UINTN        ProgressCount
  )
{
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *GraphicsOutput;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   Background;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   Accent;
  EFI_STATUS                      Status;
  UINTN                           Width;
  UINTN                           Height;
  UINTN                           TopStripeHeight;
  UINTN                           BottomStripeHeight;
  UINTN                           TagWidth;
  UINTN                           Gap;
  UINTN                           BlockWidth;
  UINTN                           Index;
  MENU_MSG_INFO                   MenuMsg;
  CHAR8                           Title[MAX_MSG_SIZE];
  CHAR8                           Subtitle[MAX_MSG_SIZE];
  UINT32                          FgColor;

  if (!IsEnableDisplayMenuFlagSupported ()) {
    return;
  }

  GraphicsOutput = NULL;
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL,
                                (VOID **)&GraphicsOutput);
  if (EFI_ERROR (Status) || GraphicsOutput == NULL ||
      GraphicsOutput->Mode == NULL || GraphicsOutput->Mode->Info == NULL) {
    return;
  }

  Width = GraphicsOutput->Mode->Info->HorizontalResolution;
  Height = GraphicsOutput->Mode->Info->VerticalResolution;
  if (Width == 0 || Height == 0) {
    return;
  }

  GetStagePixel (BgColor, &Background);
  GetStagePixel (AccentColor, &Accent);

  GraphicsOutput->Blt (GraphicsOutput, &Background, EfiBltVideoFill,
                       0, 0, 0, 0, Width, Height, 0);

  TopStripeHeight = MAX (Height / 14, 24);
  BottomStripeHeight = MAX (Height / 12, 28);
  TagWidth = MAX (Width / 7, 96);
  Gap = MAX (Width / 80, 8);
  BlockWidth = MAX ((Width - ((ProgressCount + 1) * Gap)) / MAX (ProgressCount, 1), 48);

  GraphicsOutput->Blt (GraphicsOutput, &Accent, EfiBltVideoFill,
                       0, 0, 0, 0, Width, TopStripeHeight, 0);
  GraphicsOutput->Blt (GraphicsOutput, &Accent, EfiBltVideoFill,
                       0, 0, 0, Height - BottomStripeHeight,
                       TagWidth, BottomStripeHeight, 0);

  for (Index = 0; Index < ProgressCount; ++Index) {
    UINTN DestX = Gap + (Index * (BlockWidth + Gap));
    GraphicsOutput->Blt (GraphicsOutput, &Accent, EfiBltVideoFill,
                         0, 0, DestX, Height - BottomStripeHeight,
                         BlockWidth, BottomStripeHeight, 0);
  }

  DrawMenuInit ();
  FgColor = (BgColor == BGR_BLUE || BgColor == BGR_RED) ? BGR_WHITE : BGR_BLACK;
  AsciiStrnCpyS (Title, sizeof (Title), "SM8650 DUALSTAGE",
                 AsciiStrLen ("SM8650 DUALSTAGE"));
  SetMenuMsgInfo (&MenuMsg, Title, COMMON_FACTOR, FgColor, BgColor,
                  ALIGN_LEFT, 136, NOACTION);
  DrawMenu (&MenuMsg, NULL);

  AsciiStrnCpyS (Subtitle, sizeof (Subtitle), (CHAR8 *)StageLabel,
                 AsciiStrLen (StageLabel));
  SetMenuMsgInfo (&MenuMsg, Subtitle, COMMON_FACTOR, FgColor, BgColor,
                  ALIGN_LEFT, 184, NOACTION);
  DrawMenu (&MenuMsg, NULL);
}

STATIC
VOID
ProbeRebootIf (
  IN UINT32       ProbeId,
  IN CONST CHAR8  *ProbeName
  )
{
  if (PROBE_REBOOT_STAGE_ID != ProbeId) {
    return;
  }

  DEBUG ((EFI_D_ERROR, "UEFI reboot probe %u hit at %a\n", ProbeId, ProbeName));
  RebootDevice (NORMAL_MODE);
  CpuDeadLoop ();
}

BOOLEAN
IsABRetryCountUpdateRequired (VOID)
{
  BOOLEAN BatteryStatus;

  TargetPauseForBatteryCharge (&BatteryStatus);

  if ((BatteryStatus && IsChargingScreenEnable ()) ||
      BootIntoFastboot ||
      BootIntoRecovery) {
    return FALSE;
  }
  return TRUE;
}

UINT32
GetBootDeviceType (VOID)
{
  UINTN      DataSize;
  EFI_STATUS Status;

  DataSize = sizeof (BootDeviceType);
  Status = EFI_SUCCESS;

  if (BootDeviceType == EFI_MAX_FLASH_TYPE) {
    Status = gRT->GetVariable (
                    L"SharedImemBootCfgVal",
                    &gQcomTokenSpaceGuid,
                    NULL,
                    &DataSize,
                    &BootDeviceType
                    );
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Failed to get boot device type, %r\n", Status));
    }
  }

  return BootDeviceType;
}

STATIC
EFI_STATUS
BootAndroidFromCurrentSlot (VOID)
{
  EFI_STATUS Status;
  BOOLEAN    MultiSlotBoot;
  BootInfo   Info;

  MultiSlotBoot = FALSE;
  ZeroMem (&Info, sizeof (Info));
  BootIntoFastboot = FALSE;
  BootIntoRecovery = FALSE;

  Status = DeviceInfoInit ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "DualStageLoader: DeviceInfoInit failed: %r\n",
            Status));
    return Status;
  }

  Status = EnumeratePartitions ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "DualStageLoader: EnumeratePartitions failed: %r\n",
            Status));
    return Status;
  }

  UpdatePartitionEntries ();
  MultiSlotBoot = PartitionHasMultiSlot ((CONST CHAR16 *)L"boot");
  if (MultiSlotBoot) {
    FindPtnActiveSlot ();
  }

  Status = BoardInit ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "DualStageLoader: BoardInit failed: %r\n", Status));
    return Status;
  }

  if (!GetVmData ()) {
    DEBUG ((EFI_D_ERROR, "DualStageLoader: VM Hyp calls not present\n"));
  }
  RenderStageBanner ("BOARD INIT", BGR_CYAN, BGR_BLACK, 2);
  ProbeRebootIf (12, "DualStageLoaderAfterBoardInit");

  Info.MultiSlotBoot = MultiSlotBoot;
  Info.SilentBootMode = NON_SILENT_MODE;

  RenderStageBanner ("LOAD IMAGE", BGR_YELLOW, BGR_BLACK, 3);
  ProbeRebootIf (13, "DualStageLoaderBeforeLoadImageAndAuth");
  Status = LoadImageAndAuth (&Info, FALSE, FALSE
#ifndef USE_DUMMY_BCC
                             , &BccParamsRecvdFromAVB
#endif
                            );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "DualStageLoader: LoadImageAndAuth failed: %r\n",
            Status));
    return Status;
  }

  RenderStageBanner ("BOOT LINUX", BGR_GREEN, BGR_BLACK, 4);
  ProbeRebootIf (14, "DualStageLoaderBeforeBootLinux");
  return BootLinux (&Info);
}

EFI_STATUS
EFIAPI
__attribute__ ( (no_sanitize ("safe-stack")))
DualStageLoaderEntry (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;

  (VOID)ImageHandle;
  (VOID)SystemTable;

  DEBUG ((EFI_D_INFO, "DualStageLoader starting\n"));

  Status = InitThreadUnsafeStack ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "DualStageLoader: failed to init unsafe stack: %r\n",
            Status));
    return Status;
  }

  StackGuardChkSetup ();
  RenderStageBanner ("ENTRY", BGR_BLUE, BGR_WHITE, 1);
  ProbeRebootIf (11, "DualStageLoaderEntry");
  Status = BootAndroidFromCurrentSlot ();

  __stack_chk_guard = DEFAULT_STACK_CHK_GUARD;
  DeInitThreadUnsafeStack ();
  return Status;
}
