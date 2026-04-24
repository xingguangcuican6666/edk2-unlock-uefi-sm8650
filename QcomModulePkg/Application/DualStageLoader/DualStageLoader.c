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

#define DEFAULT_STACK_CHK_GUARD 0xc0c0c0c0

BccParams_t BccParamsRecvdFromAVB = {{0}};
STATIC BOOLEAN BootIntoFastboot = FALSE;
STATIC BOOLEAN BootIntoRecovery = FALSE;
STATIC UINT32 BootDeviceType = EFI_MAX_FLASH_TYPE;

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

  Info.MultiSlotBoot = MultiSlotBoot;
  Info.SilentBootMode = NON_SILENT_MODE;

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
  Status = BootAndroidFromCurrentSlot ();

  __stack_chk_guard = DEFAULT_STACK_CHK_GUARD;
  DeInitThreadUnsafeStack ();
  return Status;
}
