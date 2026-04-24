/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of The Linux Foundation nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 *  Changes from Qualcomm Innovation Center are provided under the following license:
 *
 *  Copyright (c) 2022 - 2025 Qualcomm Innovation Center, Inc. All rights
 *  reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 *  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 *  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "AutoGen.h"
#include "BootLinux.h"
#include "BootStats.h"
#include "KeyPad.h"
#include "LinuxLoaderLib.h"
#include <FastbootLib/FastbootMain.h>
#include <Library/DeviceInfo.h>
#include <Library/DrawUI.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PartitionTableUpdate.h>
#include <Library/ShutdownServices.h>
#include <Library/StackCanary.h>
#include "Library/ThreadStack.h"
#include <Library/HypervisorMvCalls.h>
#include <Library/UpdateCmdLine.h>
#include <Protocol/EFICardInfo.h>
#include <Protocol/GraphicsOutput.h>

#define MAX_APP_STR_LEN 64
#define MAX_NUM_FS 10
#define DEFAULT_STACK_CHK_GUARD 0xc0c0c0c0
#define DUAL_STAGE_LOADER_FILE_GUID                                         \
  {                                                                         \
    0xa1168d25, 0x1f58, 0x4d2b,                                             \
    {                                                                       \
      0xae, 0x2e, 0xf2, 0xf1, 0x93, 0xb8, 0x89, 0x7c                        \
    }                                                                       \
  }

#ifndef PROBE_REBOOT_STAGE_ID
#define PROBE_REBOOT_STAGE_ID 0
#endif

#if HIBERNATION_SUPPORT_NO_AES
VOID BootIntoHibernationImage (BootInfo *Info,
                               BOOLEAN *SetRotAndBootStateAndVBH);
#endif

BccParams_t BccParamsRecvdFromAVB = {{0}};
STATIC BOOLEAN BootReasonAlarm = FALSE;
STATIC BOOLEAN BootIntoFastboot = FALSE;
STATIC BOOLEAN BootIntoRecovery = FALSE;
UINT64 FlashlessBootImageAddr = 0;
STATIC DeviceInfo DevInfo;
STATIC UINT32 BootDeviceType = EFI_MAX_FLASH_TYPE;
STATIC CONST EFI_GUID mDualStageLoaderFileGuid = DUAL_STAGE_LOADER_FILE_GUID;

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
  AsciiStrnCpyS (Title, sizeof (Title), "SM8650 LINUXLOADER",
                 AsciiStrLen ("SM8650 LINUXLOADER"));
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

// This function is used to Deactivate MDTP by entering recovery UI
STATIC EFI_STATUS MdtpDisable (VOID)
{
  BOOLEAN MdtpActive = FALSE;
  EFI_STATUS Status = EFI_SUCCESS;
  QCOM_MDTP_PROTOCOL *MdtpProtocol;

  if (FixedPcdGetBool (EnableMdtpSupport)) {
    Status = IsMdtpActive (&MdtpActive);

    if (EFI_ERROR (Status))
      return Status;

    if (MdtpActive) {
      Status = gBS->LocateProtocol (&gQcomMdtpProtocolGuid, NULL,
                                    (VOID **)&MdtpProtocol);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "Failed to locate MDTP protocol, Status=%r\n",
                Status));
        return Status;
      }
      /* Perform Local Deactivation of MDTP */
      Status = MdtpProtocol->MdtpDeactivate (MdtpProtocol, FALSE);
    }
  }

  return Status;
}

STATIC UINT8
GetRebootReason (UINT32 *ResetReason)
{
  EFI_RESETREASON_PROTOCOL *RstReasonIf;
  EFI_STATUS Status;

  Status = gBS->LocateProtocol (&gEfiResetReasonProtocolGuid, NULL,
                                (VOID **)&RstReasonIf);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error locating the reset reason protocol\n"));
    return Status;
  }

  RstReasonIf->GetResetReason (RstReasonIf, ResetReason, NULL, NULL);
  if (RstReasonIf->Revision >= EFI_RESETREASON_PROTOCOL_REVISION)
    RstReasonIf->ClearResetReason (RstReasonIf);
  return Status;
}

STATIC VOID
SetDefaultAudioFw ()
{
  CHAR8 AudioFW[MAX_AUDIO_FW_LENGTH];
  STATIC CHAR8* Src;
  STATIC CHAR8* AUDIOFRAMEWORK;
  STATIC UINT32 Length;
  EFI_STATUS Status;

  AUDIOFRAMEWORK = GetAudioFw ();
  Status = ReadAudioFrameWork (&Src, &Length);
  if ((AsciiStrCmp (Src, "audioreach") == 0) ||
                              (AsciiStrCmp (Src, "elite") == 0) ||
                              (AsciiStrCmp (Src, "awe") == 0)) {
    if (Status == EFI_SUCCESS) {
      if (AsciiStrLen (Src) == 0) {
        if (AsciiStrLen (AUDIOFRAMEWORK) > 0) {
          AsciiStrnCpyS (AudioFW, MAX_AUDIO_FW_LENGTH, AUDIOFRAMEWORK,
          AsciiStrLen (AUDIOFRAMEWORK));
          StoreAudioFrameWork (AudioFW, AsciiStrLen (AUDIOFRAMEWORK));
        }
      }
    }
    else {
      DEBUG ((EFI_D_ERROR, "AUDIOFRAMEWORK is NOT updated length =%d, %a\n",
      Length, AUDIOFRAMEWORK));
    }
  }
  else {
    if (Src != NULL) {
      Status =
      ReadWriteDeviceInfo (READ_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
      if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "Unable to Read Device Info: %r\n", Status));
       }
      gBS->SetMem (DevInfo.AudioFramework, sizeof (DevInfo.AudioFramework), 0);
      gBS->CopyMem (DevInfo.AudioFramework, AUDIOFRAMEWORK,
                                      AsciiStrLen (AUDIOFRAMEWORK));
      Status =
      ReadWriteDeviceInfo (WRITE_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
      if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "Unable to store audio framework: %r\n", Status));
        return;
      }
    }
  }
}

BOOLEAN IsABRetryCountUpdateRequired (VOID)
{
 BOOLEAN BatteryStatus;

 /* Check power off charging */
 TargetPauseForBatteryCharge (&BatteryStatus);

 /* Do not decrement bootable retry count in below states:
 * fastboot, fastbootd, charger, recovery
 */
 if ((BatteryStatus &&
 IsChargingScreenEnable ()) ||
 BootIntoFastboot ||
 BootIntoRecovery) {
  return FALSE;
 }
  return TRUE;
}

/**
  This function is used to check for boot type:
    FlashlessBoot, NetworkBoot, Fastboot.
 **/

UINT32 GetBootDeviceType ()
{
  UINTN  DataSize = sizeof (BootDeviceType);
  EFI_STATUS Status = EFI_SUCCESS;

  if (BootDeviceType == EFI_MAX_FLASH_TYPE) {
    Status = gRT->GetVariable (L"SharedImemBootCfgVal",
               &gQcomTokenSpaceGuid, NULL, &DataSize, &BootDeviceType);
    if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "Failed to get boot device type, %r\n", Status));
    }
  }

  return BootDeviceType;
}

STATIC EFI_STATUS
LaunchEmbeddedSecondStage (IN EFI_HANDLE ParentImageHandle)
{
  typedef struct {
    MEDIA_FW_VOL_FILEPATH_DEVICE_PATH FvFile;
    EFI_DEVICE_PATH_PROTOCOL          End;
  } FV_APP_DEVICE_PATH;

  EFI_STATUS                 Status;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL   *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL   *SecondStagePath;
  EFI_HANDLE                 SecondStageHandle;
  CHAR16                     *ExitData;
  UINTN                      ExitDataSize;
  FV_APP_DEVICE_PATH         FvPath;

  LoadedImage = NULL;
  SecondStagePath = NULL;
  SecondStageHandle = NULL;
  ExitData = NULL;
  ExitDataSize = 0;

  Status = gBS->HandleProtocol (ParentImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "LaunchEmbeddedSecondStage: failed to get loaded image: %r\n",
            Status));
    return Status;
  }

  if (LoadedImage->DeviceHandle == NULL) {
    DEBUG ((EFI_D_ERROR,
            "LaunchEmbeddedSecondStage: missing parent device handle\n"));
    return EFI_NOT_FOUND;
  }

  ParentDevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  if (ParentDevicePath == NULL) {
    DEBUG ((EFI_D_ERROR,
            "LaunchEmbeddedSecondStage: missing parent device path\n"));
    return EFI_NOT_FOUND;
  }

  EfiInitializeFwVolDevicepathNode (&FvPath.FvFile, &mDualStageLoaderFileGuid);
  SetDevicePathEndNode (&FvPath.End);

  SecondStagePath = AppendDevicePathNode (
                      ParentDevicePath,
                      (EFI_DEVICE_PATH_PROTOCOL *)&FvPath.FvFile);
  if (SecondStagePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->LoadImage (FALSE, ParentImageHandle, SecondStagePath,
                  NULL, 0, &SecondStageHandle);
  FreePool (SecondStagePath);

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "LaunchEmbeddedSecondStage: failed to load stage2 image: %r\n",
            Status));
    return Status;
  }

  DEBUG ((EFI_D_INFO, "Launching embedded second-stage EFI payload\n"));
  Status = gBS->StartImage (SecondStageHandle, &ExitDataSize, &ExitData);
  if (ExitData != NULL) {
    DEBUG ((EFI_D_ERROR,
            "Embedded second-stage returned: %s\n",
            ExitData));
    FreePool (ExitData);
  }

  return Status;
}

/**
  Linux Loader Application EntryPoint

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

 **/

EFI_STATUS EFIAPI  __attribute__ ( (no_sanitize ("safe-stack")))
LinuxLoaderEntry (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  UINT32 BootReason = NORMAL_MODE;
  UINT32 KeyPressed = SCAN_NULL;
  /* SilentMode Boot */
  CHAR8 SilentBootMode = NON_SILENT_MODE;
  /* MultiSlot Boot */
  BOOLEAN MultiSlotBoot = FALSE;
  /* Flashless Boot */
  BOOLEAN FlashlessBoot = FALSE;
  EFI_MEM_CARDINFO_PROTOCOL *CardInfo = NULL;
  /* set ROT, BootState and VBH only once per boot*/
  BOOLEAN SetRotAndBootStateAndVBH = FALSE;
  BOOLEAN FDRDetected = FALSE;

  DEBUG ((EFI_D_INFO, "Loader Build Info: %a %a\n", __DATE__, __TIME__));
  DEBUG ((EFI_D_VERBOSE, "LinuxLoader Load Address to debug ABL: 0x%llx\n",
         (UINTN)LinuxLoaderEntry & (~ (0xFFF))));
  DEBUG ((EFI_D_VERBOSE, "LinuxLoaderEntry Address: 0x%llx\n",
         (UINTN)LinuxLoaderEntry));

  BootStatsSetInitTimeStamp ();

  Status = InitThreadUnsafeStack ();

  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to Allocate memory for Unsafe Stack: %r\n",
            Status));
    goto stack_guard_update_default;
  }

  StackGuardChkSetup ();
  RenderStageBanner ("ENTRY", BGR_BLUE, BGR_WHITE, 1);
  ProbeRebootIf (1, "LinuxLoaderEntry");

  BootStatsSetTimeStamp (BS_BL_START);

  /* Check if memory card is present; goto flashless if not */
  Status = gBS->LocateProtocol (&gEfiMemCardInfoProtocolGuid, NULL,
                                  (VOID **)&CardInfo);
  if (EFI_ERROR (Status)) {
    FlashlessBootImageAddr = BASE_ADDRESS;
    FlashlessBoot = TRUE;
    /* In flashless boot avoid all access to secondary storage during boot */
    goto flashless_boot;
  }

  // Initialize verified boot & Read Device Info
  Status = DeviceInfoInit ();
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Initialize the device info failed: %r\n", Status));
    goto stack_guard_update_default;
  }

  Status = EnumeratePartitions ();

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "LinuxLoader: Could not enumerate partitions: %r\n",
            Status));
    goto stack_guard_update_default;
  }

  UpdatePartitionEntries ();
  /*Check for multislot boot support*/
  MultiSlotBoot = PartitionHasMultiSlot ((CONST CHAR16 *)L"boot");
  if (MultiSlotBoot) {
    DEBUG ((EFI_D_VERBOSE, "Multi Slot boot is supported\n"));
    FindPtnActiveSlot ();
  }

  Status = GetKeyPress (&KeyPressed);
  if (Status == EFI_SUCCESS) {
    if (KeyPressed == SCAN_DOWN)
      BootIntoFastboot = TRUE;
    if (KeyPressed == SCAN_UP)
      BootIntoRecovery = TRUE;
    if (KeyPressed == SCAN_ESC)
      RebootDevice (EMERGENCY_DLOAD);
  } else if (Status == EFI_DEVICE_ERROR) {
    DEBUG ((EFI_D_ERROR, "Error reading key status: %r\n", Status));
    goto stack_guard_update_default;
  }

  SetDefaultAudioFw ();

  // check for reboot mode
  Status = GetRebootReason (&BootReason);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to get Reboot reason: %r\n", Status));
    goto stack_guard_update_default;
  }

  switch (BootReason) {
  case FASTBOOT_MODE:
    BootIntoFastboot = TRUE;
    break;
  case RECOVERY_MODE:
    BootIntoRecovery = TRUE;
    break;
  case ALARM_BOOT:
    BootReasonAlarm = TRUE;
    break;
  case DM_VERITY_ENFORCING:
    // write to device info
    Status = EnableEnforcingMode (TRUE);
    if (Status != EFI_SUCCESS)
      goto stack_guard_update_default;
    break;
  case DM_VERITY_LOGGING:
    /* Disable MDTP if it's Enabled through Local Deactivation */
    Status = MdtpDisable ();
    if (EFI_ERROR (Status) && Status != EFI_NOT_FOUND) {
      DEBUG ((EFI_D_ERROR, "MdtpDisable Returned error: %r\n", Status));
      goto stack_guard_update_default;
    }
    // write to device info
    Status = EnableEnforcingMode (FALSE);
    if (Status != EFI_SUCCESS)
      goto stack_guard_update_default;

    break;
  case DM_VERITY_KEYSCLEAR:
    Status = ResetDeviceState ();
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "VB Reset Device State error: %r\n", Status));
      goto stack_guard_update_default;
    }
    break;
  case SILENT_MODE:
    SilentBootMode = SILENT_MODE;
    break;
  case NON_SILENT_MODE:
    SilentBootMode = NON_SILENT_MODE;
    break;
  case FORCED_SILENT:
    SilentBootMode = FORCED_SILENT;
    break;
  case FORCED_NON_SILENT:
    SilentBootMode = FORCED_NON_SILENT;
    break;
  default:
    if (BootReason != NORMAL_MODE) {
      DEBUG ((EFI_D_ERROR,
             "Boot reason: 0x%x not handled, defaulting to Normal Boot\n",
             BootReason));
    }
    break;
  }

  Status = RecoveryInit (&BootIntoRecovery);
  if (Status != EFI_SUCCESS)
    DEBUG ((EFI_D_VERBOSE, "RecoveryInit failed ignore: %r\n", Status));

   if (BootIntoRecovery) {
    Status = DetectFDR (&FDRDetected);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "DetectFDR failed: %r\n", Status));
    }
    if (FDRDetected) {
      DEBUG ((EFI_D_INFO, "LinuxloaderEntry: FDRDetected\n"));
      Status = SetFDRFlag ();
      if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "SetFDRFlag failed: %r\n", Status));
      }
    }
  }

flashless_boot:
  /* Populate board data required for fastboot, dtb selection and cmd line */
  Status = BoardInit ();
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error finding board information: %r\n", Status));
    return Status;
  }
  RenderStageBanner ("BOARD INIT", BGR_CYAN, BGR_BLACK, 2);
  ProbeRebootIf (2, "LinuxLoaderAfterBoardInit");

  DEBUG ((EFI_D_INFO, "KeyPress:%u, BootReason:%u\n", KeyPressed, BootReason));
  DEBUG ((EFI_D_INFO, "Fastboot=%d, Recovery:%d\n",
                                          BootIntoFastboot, BootIntoRecovery));
  DEBUG ((EFI_D_INFO, "SilentBoot Mode:%u\n", SilentBootMode));
  if (!GetVmData ()) {
    DEBUG ((EFI_D_ERROR, "VM Hyp calls not present\n"));
  }

  if (BootIntoFastboot) {
      goto fastboot;
  }
  else {
    RenderStageBanner ("STAGE2 HANDOFF", BGR_ORANGE, BGR_BLACK, 3);
    ProbeRebootIf (3, "LinuxLoaderBeforeEmbeddedStage2");
    if (!BootIntoRecovery && !FlashlessBoot) {
      Status = LaunchEmbeddedSecondStage (ImageHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR,
                "Embedded second-stage failed, falling back: %r\n",
                Status));
      } else {
        DEBUG ((EFI_D_INFO,
                "Embedded second-stage returned control, falling back\n"));
      }
    }

    BootInfo Info = {0};
    Info.MultiSlotBoot = MultiSlotBoot;
    Info.BootIntoRecovery = BootIntoRecovery;
    Info.BootReasonAlarm = BootReasonAlarm;
    Info.FlashlessBoot = FlashlessBoot;
    Info.SilentBootMode = SilentBootMode;
    RenderStageBanner ("LOAD IMAGE", BGR_YELLOW, BGR_BLACK, 4);
    ProbeRebootIf (4, "LinuxLoaderBeforeLoadImageAndAuth");
  #if HIBERNATION_SUPPORT_NO_AES
    BootIntoHibernationImage (&Info, &SetRotAndBootStateAndVBH);
  #endif
    Status = LoadImageAndAuth (&Info, FALSE, SetRotAndBootStateAndVBH
  #ifndef USE_DUMMY_BCC
                               , &BccParamsRecvdFromAVB
  #endif
                              );
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "LoadImageAndAuth failed: %r\n", Status));
      goto fastboot;
    }

    RenderStageBanner ("BOOT LINUX", BGR_GREEN, BGR_BLACK, 5);
    ProbeRebootIf (5, "LinuxLoaderBeforeBootLinux");
    BootLinux (&Info);
  }

fastboot:
  RenderStageBanner ("FASTBOOT FALLBACK", BGR_RED, BGR_WHITE, 6);
#ifdef AUTO_VIRT_ABL
  DEBUG ((EFI_D_INFO, "Rebooting the device.\n"));
  RebootDevice (NORMAL_MODE);
#endif
  if (FlashlessBoot) {
    DEBUG ((EFI_D_ERROR, "No fastboot support for flashless chipsets,"
                               " Infinte loop\n"));
    while (1);
  }
  DEBUG ((EFI_D_INFO, "Launching fastboot\n"));
  Status = FastbootInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Launch Fastboot App: %d\n", Status));
    goto stack_guard_update_default;
  }

stack_guard_update_default:
  /*Update stack check guard with defualt value then return*/
  __stack_chk_guard = DEFAULT_STACK_CHK_GUARD;

  DeInitThreadUnsafeStack ();

  return Status;
}
