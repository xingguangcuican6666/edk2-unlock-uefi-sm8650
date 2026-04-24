/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Changes from Qualcomm Innovation Center, Inc. are provided
// under the following license:
// Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "KeymasterClient.h"
#include "VerifiedBoot.h"
#include <Library/BaseMemoryLib.h>
#include <Library/Debug.h>
#include <Library/DebugLib.h>
#include <Library/DeviceInfo.h>
#include <Library/LinuxLoaderLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/EFIQseecom.h>
#include <Protocol/EFISPSS.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/scm_sip_interface.h>

#if AUTO_VIRT_ABL
/* SMCI */
#include <Protocol/EFIScm.h>
#include "SmciInvokeUtils.h"
#define CKMHal_UID 150
#define CAppClient_UID 0x97U
static Object AppObj = Object_NULL;
#else
/* QSEECOM */
typedef struct {
  QCOM_QSEECOM_PROTOCOL *QseeComProtocol;
  UINT32 AppId;
} KMHandle;

STATIC KMHandle Handle = {NULL};
STATIC KeymintSharedInfoStruct SPUKeymintSharedInfo;
#endif

/**
 * KM Commands supported
 */
#define KEYMASTER_CMD_ID_OLD 0UL
#define KEYMASTER_CMD_ID 0x100UL
#define KEYMASTER_UTILS_CMD_ID 0x200UL
#define GK_CMD_ID 0x1000UL

typedef enum {
  /*
   * List the commands supportedin by the hardware.
   */
  KEYMASTER_GET_SUPPORTED_ALGORITHMS = (KEYMASTER_CMD_ID + 1UL),
  KEYMASTER_GET_SUPPORTED_BLOCK_MODES = (KEYMASTER_CMD_ID + 2UL),
  KEYMASTER_GET_SUPPORTED_PADDING_MODES = (KEYMASTER_CMD_ID + 3UL),
  KEYMASTER_GET_SUPPORTED_DIGESTS = (KEYMASTER_CMD_ID + 4UL),
  KEYMASTER_GET_SUPPORTED_IMPORT_FORMATS = (KEYMASTER_CMD_ID + 5UL),
  KEYMASTER_GET_SUPPORTED_EXPORT_FORMATS = (KEYMASTER_CMD_ID + 6UL),
  KEYMASTER_ADD_RNG_ENTROPY = (KEYMASTER_CMD_ID + 7UL),
  KEYMASTER_GENERATE_KEY = (KEYMASTER_CMD_ID + 8UL),
  KEYMASTER_GET_KEY_CHARACTERISTICS = (KEYMASTER_CMD_ID + 9UL),
  KEYMASTER_RESCOPE = (KEYMASTER_CMD_ID + 10UL),
  KEYMASTER_IMPORT_KEY = (KEYMASTER_CMD_ID + 11UL),
  KEYMASTER_EXPORT_KEY = (KEYMASTER_CMD_ID + 12UL),
  KEYMASTER_DELETE_KEY = (KEYMASTER_CMD_ID + 13UL),
  KEYMASTER_DELETE_ALL_KEYS = (KEYMASTER_CMD_ID + 14UL),
  KEYMASTER_BEGIN = (KEYMASTER_CMD_ID + 15UL),
  KEYMASTER_UPDATE = (KEYMASTER_CMD_ID + 17UL),
  KEYMASTER_FINISH = (KEYMASTER_CMD_ID + 18UL),
  KEYMASTER_ABORT = (KEYMASTER_CMD_ID + 19UL),
  KEYMASTER_UPGRADE = (KEYMASTER_CMD_ID + 20UL),
  KEYMASTER_ATTEST = (KEYMASTER_CMD_ID + 21UL),
  KEYMASTER_CONFIGURE = (KEYMASTER_CMD_ID + 22UL),

  KEYMASTER_GET_VERSION = (KEYMASTER_UTILS_CMD_ID + 0UL),
  KEYMASTER_SET_ROT = (KEYMASTER_UTILS_CMD_ID + 1UL),
  KEYMASTER_READ_KM_DEVICE_STATE = (KEYMASTER_UTILS_CMD_ID + 2UL),
  KEYMASTER_WRITE_KM_DEVICE_STATE = (KEYMASTER_UTILS_CMD_ID + 3UL),
  KEYMASTER_MILESTONE_CALL = (KEYMASTER_UTILS_CMD_ID + 4UL),
  KEYMASTER_GET_AUTH_TOKEN_KEY = (KEYMASTER_UTILS_CMD_ID + 5UL),
  KEYMASTER_SECURE_WRITE_PROTECT = (KEYMASTER_UTILS_CMD_ID + 6UL),
  KEYMASTER_SET_VERSION = (KEYMASTER_UTILS_CMD_ID + 7UL),
  KEYMASTER_SET_BOOT_STATE = (KEYMASTER_UTILS_CMD_ID + 8UL),
  KEYMASTER_PROVISION_ATTEST_KEY = (KEYMASTER_UTILS_CMD_ID + 9UL),
  KEYMASTER_SET_VBH = (KEYMASTER_UTILS_CMD_ID + 17UL),
  KEYMASTER_GET_DATE_SUPPORT = (KEYMASTER_UTILS_CMD_ID + 21UL),
  KEYMASTER_FBE_SET_SEED = (KEYMASTER_UTILS_CMD_ID + 24UL),
  KEYMINT_GENERATE_FRS_AND_UDS = (KEYMASTER_UTILS_CMD_ID + 25UL),

  KEYMASTER_LAST_CMD_ENTRY = (int)0xFFFFFFFFULL
} KeyMasterCmd;

typedef enum {
  KM_ERROR_INVALID_TAG = -40,
} KeyMasterError;

typedef struct {
  INT32 Status;
} __attribute__ ((packed)) KMSetRotRsp;

typedef struct {
  INT32 Status;
} __attribute__ ((packed)) KMSetBootStateRsp;

typedef struct {
  UINT32 CmdId;
} __attribute__ ((packed)) KMGetVersionReq;

typedef struct {
  INT32 Status;
  UINT32 Major;
  UINT32 Minor;
  UINT32 AppMajor;
  UINT32 AppMinor;
} __attribute__ ((packed)) KMGetVersionRsp;

typedef struct {
  INT32 Status;
} __attribute__ ((packed)) KMSetVbhRsp;

typedef struct {
  UINT32 CmdId;
} __attribute__ ((packed)) KMGetDateSupportReq;

typedef struct {
  INT32 Status;
} __attribute__ ((packed)) KMGetDateSupportRsp;

typedef struct {
  UINT32 CmdId;
} __attribute__ ((packed)) KMFbeSetSeedReq;

typedef struct {
  INT32 Status;
} __attribute__ ((packed)) KMFbeSetSeedRsp;

typedef struct {
  UINT32 FrsSecLen;                 /*Holds length of FRS secret*/
  uint8_t FrsSec[DICE_HIDDEN_SIZE]; /*Holds plain secret*/
} __attribute__ ((packed)) KMFrsSec;

typedef struct {
  UINT32 CmdId;
  UINT32 FdrFlag;
  KMFrsSec FrsSecData;
} __attribute__ ((packed)) KMGetFRSUDSReq;

typedef struct {
  INT32 Status;
  UINT32 FrsLen;
  UINT32 UdsLen;
  uint8_t Frs[DICE_HIDDEN_SIZE];    /*Factory reset Secret */
  uint8_t Uds[DICE_CDI_SIZE];       /*Unique Device Secret*/
  UINT32 FrsSecLen;                 /*Holds length of FRS secret*/
  uint8_t FrsSec[DICE_HIDDEN_SIZE]; /*Holds plain secret*/
} __attribute__ ((packed)) KMGetFRSUDSRsp;

#ifndef AUTO_VIRT_ABL
STATIC EFI_STATUS ShareKeyMintInfoWithSPU (VOID);

EFI_STATUS
KeyMasterStartApp (KMHandle *Handle)
{
  EFI_STATUS Status = EFI_SUCCESS;
  KMGetVersionReq Req = {0};
  KMGetVersionRsp Rsp = {0};

  if (Handle == NULL) {
    DEBUG ((EFI_D_ERROR, "KeyMasterStartApp: Invalid Handle\n"));
    return EFI_INVALID_PARAMETER;
  }

  if (Handle->QseeComProtocol != NULL) {
    return Status;
  }

  Status = gBS->LocateProtocol (&gQcomQseecomProtocolGuid, NULL,
                                (VOID **)&(Handle->QseeComProtocol));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to locate QSEECom protocol: %r\n", Status));
    return Status;
  }

  Status = Handle->QseeComProtocol->QseecomStartApp (
      Handle->QseeComProtocol, "keymaster", &(Handle->AppId));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterStartApp: QseecomStartApp failed status: %r\n", Status));
    return Status;
  }

  DEBUG ((EFI_D_VERBOSE, "keymaster app id %d\n", Handle->AppId));

  Req.CmdId = KEYMASTER_GET_VERSION;
  Status = Handle->QseeComProtocol->QseecomSendCmd (
      Handle->QseeComProtocol, Handle->AppId, (UINT8 *)&Req, sizeof (Req),
      (UINT8 *)&Rsp, sizeof (Rsp));
  if (Status != EFI_SUCCESS || Rsp.Status != 0 || Rsp.Major < 2) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterStartApp: Get Version err, status: "
            "%d, response status: %d, Major: %d\n",
            Status, Rsp.Status, Rsp.Major));
    return EFI_LOAD_ERROR;
  }
  DEBUG ((EFI_D_VERBOSE, "KeyMasterStartApp success AppId: 0x%x, Major: %d\n",
          Handle->AppId, Rsp.Major));
  return Status;
}
#else /*SMCI*/
EFI_STATUS
KeyMasterStartApp (VOID)
{
  EFI_STATUS Status = EFI_SUCCESS;
  KMGetVersionReq Req = {0};
  KMGetVersionRsp Rsp = {0};

  Object ClientEnvObj = Object_NULL;
  Object AppClientObj = Object_NULL;
  Object AppOpenerObj = Object_NULL;

  if (!Object_isNull (AppObj)) {
    return EFI_SUCCESS;
  }

  size_t RspLenOut = 0;
  CONST CHAR8 *kmAppName = "keymaster64";

  QCOM_SCM_PROTOCOL *pQcomScmProtocol = NULL;
  // Locate QCOM_SCM_PROTOCOL.
  Status = gBS->LocateProtocol (&gQcomScmProtocolGuid, NULL,
                                (VOID **)&pQcomScmProtocol);
  if (Status != EFI_SUCCESS ||
      (pQcomScmProtocol == NULL)) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterStartApp: Locate SCM Protocol failed, Status: (0x%x)\n",
            Status));
    Status = ERROR_SECURITY_STATE;
    return Status;
  }

  Status = pQcomScmProtocol->ScmGetClientEnv (pQcomScmProtocol, &ClientEnvObj);
  if (Object_isERROR (Status) ||
      Object_isNull (ClientEnvObj)) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterStartApp: Failed to get Client Env, Status: (0x%x)\n",
            Status));
    goto out;
  }

  Status = IClientEnvOpen (ClientEnvObj, CAppClient_UID, &AppClientObj);
  if (Object_isERROR (Status) ||
      Object_isNull (AppClientObj)) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterStartApp: Failed to get App Client, Status: (0x%x)\n",
            Status));
    goto out;
  }

  Status = IAppClientGetAppObject (AppClientObj, kmAppName,
                                   AsciiStrLen (kmAppName), &AppOpenerObj);
  if (Object_isERROR (Status) ||
      Object_isNull (AppOpenerObj)) {
    DEBUG (
        (EFI_D_ERROR,
         "KeyMasterStartApp: Failed to get App Opener Object, Status: (0x%x)\n",
         Status));
    goto out;
  }

  Status = IOpener_open (AppOpenerObj, CKMHal_UID, &AppObj);
  if (Object_isERROR (Status) ||
      Object_isNull (AppObj)) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterStartApp: Failed to get KM TA object, Status: (0x%x)\n",
            Status));
    goto out;
  }

  DEBUG ((EFI_D_INFO, "Keymaster app is loaded and ready to be used\n"));

  Req.CmdId = KEYMASTER_GET_VERSION;

  Status = IKMHal_sendCmd (AppObj, (CONST VOID *)&Req, sizeof (Req),
                           (VOID *)&Rsp, sizeof (Rsp), &RspLenOut);

  if (Status != 0 ||
      Rsp.Status != 0 ||
      Rsp.Major < 2) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterStartApp: Get Version err, Status: "
            "%d, response status: %d, Major: %d\n",
            Status, Rsp.Status, Rsp.Major));
    Status = EFI_LOAD_ERROR;
    goto out;
  }

  DEBUG ((EFI_D_INFO, "KeyMasterStartApp success, Major: %d\n", Rsp.Major));
  Status = EFI_SUCCESS;
  goto out_success;

out:
  Object_ASSIGN_NULL (AppObj);

out_success:
  Object_ASSIGN_NULL (ClientEnvObj);
  Object_ASSIGN_NULL (AppClientObj);
  Object_ASSIGN_NULL (AppOpenerObj);

  return Status;
}
#endif
EFI_STATUS
KeyMasterSetRotAndBootState (KMRotAndBootState *BootState)
{
  EFI_STATUS Status = EFI_SUCCESS;
  CHAR8 *RotDigest = NULL;
  CHAR8 *BootStateDigest = NULL;
  CHAR8 BootStateOrgangeDigest[AVB_SHA256_DIGEST_SIZE] = {0};
  AvbSHA256Ctx RotCtx;
  AvbSHA256Ctx BootStateCtx;
  KMSetRotReq RotReq = {0};
  KMSetRotRsp RotRsp = {0};
  KMSetBootStateReq BootStateReq = {0};
  KMSetBootStateRsp BootStateRsp = {0};
#ifndef AUTO_VIRT_ABL
  BOOLEAN secure_device = FALSE;
#endif
  if (BootState == NULL) {
    DEBUG ((EFI_D_ERROR, "Invalid parameter BootState\n"));
    return EFI_INVALID_PARAMETER;
  }

  /* Compute ROT digest */
  avb_sha256_init (&RotCtx);

  switch (BootState->Color) {
  case GREEN:
  case YELLOW:
    avb_sha256_update (&RotCtx, (CONST uint8_t *)BootState->PublicKey,
                       BootState->PublicKeyLength);
    avb_sha256_update (&RotCtx, (CONST uint8_t *)&BootState->IsUnlocked,
                       sizeof (BootState->IsUnlocked));
    break;
  case ORANGE:
    avb_sha256_update (&RotCtx, (CONST uint8_t *)&BootState->IsUnlocked,
                       sizeof (BootState->IsUnlocked));
    break;
  case RED:
  default:
    DEBUG ((EFI_D_ERROR, "Invalid state to boot!\n"));
    return EFI_LOAD_ERROR;
  }
  /* RotDigest is a fixed size array, cannot be NULL */
  RotDigest = (CHAR8 *)avb_sha256_final (&RotCtx);

  /* Compute BootState digest */
  switch (BootState->Color) {
  case GREEN:
  case YELLOW:
    avb_sha256_init (&BootStateCtx);
    avb_sha256_update (&BootStateCtx,
                 (CONST uint8_t *)BootState->PublicKey,
                  BootState->PublicKeyLength);
    /* BootStateDigest is a fixed size array, cannot be NULL */
    BootStateDigest = (CHAR8 *)avb_sha256_final (&BootStateCtx);
    break;
  case ORANGE:
    BootStateDigest = BootStateOrgangeDigest;
    break;
  case RED:
  default:
    DEBUG ((EFI_D_ERROR, "Invalid state to boot!\n"));
    return EFI_LOAD_ERROR;
  }

  /* Load KeyMaster App */
#ifndef AUTO_VIRT_ABL
  GUARD (KeyMasterStartApp (&Handle));
#else /*SMCI*/
  GUARD (KeyMasterStartApp ());
#endif
  /* Set ROT */
  RotReq.CmdId = KEYMASTER_SET_ROT;
  RotReq.RotOffset = (UINT8 *)&RotReq.RotDigest - (UINT8 *)&RotReq;
  RotReq.RotSize = sizeof (RotReq.RotDigest);
  CopyMem (RotReq.RotDigest, RotDigest, AVB_SHA256_DIGEST_SIZE);

#ifndef AUTO_VIRT_ABL
  SPUKeymintSharedInfo.RootOfTrust = RotReq;

  Status = Handle.QseeComProtocol->QseecomSendCmd (
      Handle.QseeComProtocol, Handle.AppId, (UINT8 *)&RotReq, sizeof (RotReq),
      (UINT8 *)&RotRsp, sizeof (RotRsp));
#else /*SMCI*/
  size_t RspLenOut = 0;
  Status = IKMHal_sendCmd (AppObj, (CONST VOID *)&RotReq, sizeof (RotReq),
                           (VOID *)&RotRsp, sizeof (RotRsp), &RspLenOut);
#endif
  if (Status != EFI_SUCCESS ||
      RotRsp.Status != 0) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterSendRotAndBootState: Set ROT err, "
            "Status: %r, response status: %d\n",
            Status, RotRsp.Status));
    return EFI_LOAD_ERROR;
  }
  DEBUG ((EFI_D_INFO, "KeyMasterSetRotAndBootState Set RoT success\n"));

  /* Set Boot State */
  BootStateReq.CmdId = KEYMASTER_SET_BOOT_STATE;
  BootStateReq.Version = 0;
  BootStateReq.Size = sizeof (BootStateReq.BootState);
  BootStateReq.Offset =
      (UINT8 *)&BootStateReq.BootState - (UINT8 *)&BootStateReq;
  BootStateReq.BootState.Color = BootState->Color;
  BootStateReq.BootState.IsUnlocked = BootState->IsUnlocked;
  BootStateReq.BootState.SystemSecurityLevel = BootState->SystemSecurityLevel;
  BootStateReq.BootState.SystemVersion = BootState->SystemVersion;
  CopyMem (BootStateReq.BootState.PublicKey, BootStateDigest,
           AVB_SHA256_DIGEST_SIZE);
#ifndef AUTO_VIRT_ABL
  SPUKeymintSharedInfo.BootInfo = BootStateReq;

  Status = Handle.QseeComProtocol->QseecomSendCmd (
      Handle.QseeComProtocol, Handle.AppId, (UINT8 *)&BootStateReq,
      sizeof (BootStateReq), (UINT8 *)&BootStateRsp, sizeof (BootStateRsp));
#else /*SMCI*/
  Status = IKMHal_sendCmd (AppObj, (CONST VOID *)&BootStateReq,
                           sizeof (BootStateReq), (VOID *)&BootStateRsp,
                           sizeof (BootStateRsp), &RspLenOut);
#endif
  if (Status != EFI_SUCCESS ||
      BootStateRsp.Status != 0) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterSendRotAndBootState: Set BootState err, "
            "Status: %r, response status: %d\n",
            Status, BootStateRsp.Status));
    return EFI_LOAD_ERROR;
  }
  DEBUG ((EFI_D_INFO, "KeyMasterSetRotAndBootState Set Boot State success\n"));

#ifndef AUTO_VIRT_ABL
  /* Provide boot tamper state to TZ */
  if (((Status = IsSecureDevice (&secure_device)) == EFI_SUCCESS) &&
      secure_device && (BootState->Color != GREEN)) {
    if (AllowSetFuse ()) {
      Status = SetFuse (TZ_HLOS_IMG_TAMPER_FUSE);
      if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "KeyMasterSetRotAndBootState: "
                             "SetFuse (TZ_HLOS_IMG_TAMPER_FUSE) fails!\n"));
        return Status;
      }
      Status = SetFuse (TZ_HLOS_TAMPER_NOTIFY_FUSE);
      if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "KeyMasterSetRotAndBootState: "
                             "SetFuse (TZ_HLOS_TAMPER_NOTIFY_FUSE) fails!\n"));
        return Status;
      }
    }
  }
#endif

  DEBUG ((EFI_D_VERBOSE, "KeyMasterSetRotAndBootState success\n"));
  return Status;
}

#ifndef AUTO_VIRT_ABL
EFI_STATUS
SetVerifiedBootHash (CONST CHAR8 *Vbh, UINTN VbhSize)
{
  EFI_STATUS Status = EFI_SUCCESS;
  KMSetVbhReq VbhReq = {0};
  KMSetVbhRsp VbhRsp = {0};

  if (!Vbh ||
      VbhSize != sizeof (VbhReq.Vbh)) {
    DEBUG ((EFI_D_ERROR, "Vbh input params invalid\n"));
    return EFI_INVALID_PARAMETER;
  }

  /* Load KeyMaster App */
  GUARD (KeyMasterStartApp (&Handle));
  VbhReq.CmdId = KEYMASTER_SET_VBH;
  CopyMem (VbhReq.Vbh, Vbh, VbhSize);
  SPUKeymintSharedInfo.Vbh = VbhReq;

  Status = Handle.QseeComProtocol->QseecomSendCmd (
      Handle.QseeComProtocol, Handle.AppId, (UINT8 *)&VbhReq, sizeof (VbhReq),
      (UINT8 *)&VbhRsp, sizeof (VbhRsp));

  if (Status != EFI_SUCCESS ||
      VbhRsp.Status != 0) {
    DEBUG ((EFI_D_ERROR,
            "Set Vbh Error, "
            "Status: %r, response status: %d\n",
            Status, VbhRsp.Status));
    if (Status == EFI_SUCCESS &&
        VbhRsp.Status == KM_ERROR_INVALID_TAG) {
      DEBUG ((EFI_D_ERROR, "VBH not supported in keymaster\n"));
      return EFI_SUCCESS;
    }
    return EFI_LOAD_ERROR;
  }

  Status = ShareKeyMintInfoWithSPU ();

  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "ShareKeyMintInfoWithSPU failed: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}
#else /*SMCI*/
EFI_STATUS
SetVerifiedBootHash (CONST CHAR8 *Vbh, UINTN VbhSize)
{
  EFI_STATUS Status = EFI_SUCCESS;
  KMSetVbhReq VbhReq = {0};
  KMSetVbhRsp VbhRsp = {0};

  if (!Vbh ||
      VbhSize != sizeof (VbhReq.Vbh)) {
    DEBUG ((EFI_D_ERROR, "Vbh input params invalid\n"));
    return EFI_INVALID_PARAMETER;
  }

  /* Load KeyMaster App */
  GUARD (KeyMasterStartApp ());
  VbhReq.CmdId = KEYMASTER_SET_VBH;
  CopyMem (VbhReq.Vbh, Vbh, VbhSize);

  size_t RspLenOut = 0;
  /* Load KeyMaster App */
  GUARD (KeyMasterStartApp ());
  Status = IKMHal_sendCmd (AppObj, (CONST VOID *)&VbhReq, sizeof (VbhReq),
                           (VOID *)&VbhRsp, sizeof (VbhRsp), &RspLenOut);

  if (Status != EFI_SUCCESS ||
      VbhRsp.Status != 0) {
    DEBUG ((EFI_D_ERROR,
            "Set Vbh Error, "
            "Status: %r, response status: %d\n",
            Status, VbhRsp.Status));
    if (Status == EFI_SUCCESS &&
        VbhRsp.Status == KM_ERROR_INVALID_TAG) {
      DEBUG ((EFI_D_ERROR, "VBH not supported in keymaster\n"));
      return EFI_SUCCESS;
    }
    return EFI_LOAD_ERROR;
  }

  return EFI_SUCCESS;
}
#endif

#if AVB_ENABLE_LEGACY_FEATURE
/* To use the code protected by this flag, add the flag to
 * GCC:*_*_*_CC_FLAGS  in AvbLib.inf*/
EFI_STATUS
KeyMasterGetDateSupport (BOOLEAN *Supported)
{
  EFI_STATUS Status = EFI_SUCCESS;
  KMGetDateSupportReq Req = {0};
  KMGetDateSupportRsp Rsp = {0};
  Req.CmdId = KEYMASTER_GET_DATE_SUPPORT;
#ifndef AUTO_VIRT_ABL
  GUARD (KeyMasterStartApp (&Handle));
  Status = Handle.QseeComProtocol->QseecomSendCmd (
      Handle.QseeComProtocol, Handle.AppId, (UINT8 *)&Req, sizeof (Req),
      (UINT8 *)&Rsp, sizeof (Rsp));
#else /*SMCI*/
  GUARD (KeyMasterStartApp ());
  size_t RspLenOut = 0;
  Status = IKMHal_sendCmd (AppObj, (CONST VOID *)&Req, sizeof (Req),
                           (VOID *)&Rsp, sizeof (Rsp), &RspLenOut);
#endif
  if (Status != EFI_SUCCESS ||
      Rsp.Status != 0) {
    DEBUG ((EFI_D_ERROR,
            "Keymaster: Get date support error, status: "
            "%d, response status: %d\n",
            Status, Rsp.Status));
    if (Status == EFI_SUCCESS &&
        Rsp.Status == KM_ERROR_INVALID_TAG) {
      DEBUG ((EFI_D_ERROR, "Date in patch level not supported in keymaster\n"));
      *Supported = FALSE;
      return EFI_SUCCESS;
    }
    return EFI_LOAD_ERROR;
  }

  *Supported = TRUE;
  return Status;
}
#endif

EFI_STATUS
KeyMasterSetRotForLE (KMRotAndBootStateForLE *BootState)
{
  EFI_STATUS Status = EFI_SUCCESS;
  CHAR8 *RotDigest = NULL;
  AvbSHA256Ctx RotCtx;
  KMSetRotReq RotReq = {0};
  KMSetRotRsp RotRsp = {0};

  if (BootState == NULL) {
    DEBUG ((EFI_D_ERROR, "Invalid parameter BootState\n"));
    return EFI_INVALID_PARAMETER;
  }

  /* Compute ROT digest */
  avb_sha256_init (&RotCtx);
  avb_sha256_update (&RotCtx, BootState->PublicKeyMod,
                     BootState->PublicKeyModLength);
  avb_sha256_update (&RotCtx, BootState->PublicKeyExp,
                     BootState->PublicKeyExpLength);
  avb_sha256_update (&RotCtx, &BootState->IsUnlocked,
                     sizeof (BootState->IsUnlocked));
  /* RotDigest is a fixed size array, cannot be NULL */
  RotDigest = (CHAR8 *)avb_sha256_final (&RotCtx);
  if (!*RotDigest) {
    DEBUG ((EFI_D_ERROR, "Failed to set ROT Digest\n"));
    return EFI_INVALID_PARAMETER;
  }
  /* Load KeyMaster App */
#ifndef AUTO_VIRT_ABL
  GUARD (KeyMasterStartApp (&Handle));
#else /*SMCI*/
  GUARD (KeyMasterStartApp ());
#endif

  /* Set ROT */
  RotReq.CmdId = KEYMASTER_SET_ROT;
  RotReq.RotOffset = (UINT8 *)&RotReq.RotDigest - (UINT8 *)&RotReq;
  RotReq.RotSize = sizeof (RotReq.RotDigest);
  CopyMem (RotReq.RotDigest, RotDigest, AVB_SHA256_DIGEST_SIZE);
#ifndef AUTO_VIRT_ABL
  Status = Handle.QseeComProtocol->QseecomSendCmd (
      Handle.QseeComProtocol, Handle.AppId, (UINT8 *)&RotReq, sizeof (RotReq),
      (UINT8 *)&RotRsp, sizeof (RotRsp));
#else /*SMCI*/
  size_t RspLenOut = 0;
  Status = IKMHal_sendCmd (AppObj, (CONST VOID *)&RotReq, sizeof (RotReq),
                           (VOID *)&RotRsp, sizeof (RotRsp), &RspLenOut);
#endif
  if (Status != EFI_SUCCESS ||
      RotRsp.Status != 0) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterSendRotAndBootState: Set ROT err, "
            "Status: %r, response status: %d\n",
            Status, RotRsp.Status));
    return EFI_LOAD_ERROR;
  }

  DEBUG ((EFI_D_INFO, "KeyMasterSetRotForLE success\n"));
  return Status;
}

EFI_STATUS
KeyMasterFbeSetSeed (VOID)
{
  EFI_STATUS Status = EFI_SUCCESS;
  KMFbeSetSeedReq Req = {0};
  KMFbeSetSeedRsp Rsp = {0};
  /* Load KeyMaster App */
#ifndef AUTO_VIRT_ABL
  GUARD (KeyMasterStartApp (&Handle));
#else /*SMCI*/
  GUARD (KeyMasterStartApp ());
#endif
  Req.CmdId = KEYMASTER_FBE_SET_SEED;
#ifndef AUTO_VIRT_ABL
  Status = Handle.QseeComProtocol->QseecomSendCmd (
      Handle.QseeComProtocol, Handle.AppId, (UINT8 *)&Req, sizeof (Req),
      (UINT8 *)&Rsp, sizeof (Rsp));
#else /*SMCI*/
  size_t RspLenOut = 0;
  Status = IKMHal_sendCmd (AppObj, (CONST VOID *)&Req, sizeof (Req),
                           (VOID *)&Rsp, sizeof (Rsp), &RspLenOut);
#endif
  if (Status != EFI_SUCCESS ||
      Rsp.Status != 0) {
    DEBUG ((EFI_D_ERROR,
            "Keymaster: fbe set seed error, status: "
            "%d, response status: %d\n",
            Status, Rsp.Status));
    return EFI_LOAD_ERROR;
  }

  return Status;
}

#ifndef AUTO_VIRT_ABL
STATIC EFI_STATUS
ShareKeyMintInfoWithSPU (VOID)
{
  SpssProtocol *SPSSProtocol;
  EFI_STATUS Status = gBS->LocateProtocol (&gEfiSPSSProtocolGuid, NULL,
                                           (VOID **)&(SPSSProtocol));

  if (Status == EFI_NOT_FOUND) {
    /* This chipset doesn't have support for sharing keymint info */
    DEBUG ((EFI_D_VERBOSE, "No SPSS EFI protocol, not sharing keymint info\n"));
    return EFI_SUCCESS;
  } else if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to locate SPSS protocol: %r\n", Status));
    return Status;
  }

  Status = SPSSProtocol->SPSSDxe_ShareKeyMintInfo (&SPUKeymintSharedInfo);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "SPSSDxe_ShareKeyMintInfo failed: %r\n", Status));
  }

  // Clear data from memory
  SetMem ((VOID *)(&SPUKeymintSharedInfo), sizeof (SPUKeymintSharedInfo), 0);

  return Status;
}
#endif

/* KeyMasterGetFRSAndUDS will fetch Unique Device Secret(UDS) and
 * Factory Reset Sequence(FRS) from Keymint.
 */
EFI_STATUS
KeyMasterGetFRSAndUDS (BccParams_t *bcc_params)
{
  EFI_STATUS Status = EFI_SUCCESS;
  KMGetFRSUDSReq FRSUDSReq = {0};
  KMGetFRSUDSRsp FRSUDSRsp = {0};
  DeviceInfo *Devinfo = NULL;

  if (bcc_params == NULL ||
      bcc_params->FRS == NULL ||
      bcc_params->UDS == NULL) {
    DEBUG ((EFI_D_ERROR, "KeyMasterGetFRSAndUDS: Parameter received"
                         "is NULL"));
    Status = EFI_INVALID_PARAMETER;
    goto out;
  }

  Devinfo = AllocateZeroPool (sizeof (DeviceInfo));
  if (Devinfo == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to allocate zero pool for device info.\n"));
    goto out;
  }

  /* Load KeyMaster App */
#ifndef AUTO_VIRT_ABL
  GUARD (KeyMasterStartApp (&Handle));
#else /*SMCI*/
  GUARD (KeyMasterStartApp ());
#endif
  FRSUDSReq.CmdId = KEYMINT_GENERATE_FRS_AND_UDS;

  /* Read FDR Flag and FRS secret from DevInfo */
  Status =
      ReadWriteDeviceInfo (READ_CONFIG, (VOID *)Devinfo, sizeof (DeviceInfo));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterGetFRSAndUDS: Unable to Read Device Info:"
            " %r\n",
            Status));
    goto out;
  }

  FRSUDSReq.FdrFlag = Devinfo->FdrFlag;
  if (Devinfo->FrsSecLen != 0) {
    /* If any FRS secret is maintained in DevInfo,
     * copy it into request buffer.
     */
    FRSUDSReq.FrsSecData.FrsSecLen = Devinfo->FrsSecLen;
    CopyMem (FRSUDSReq.FrsSecData.FrsSec, Devinfo->FrsSec,
             FRSUDSReq.FrsSecData.FrsSecLen);
  }
#ifndef AUTO_VIRT_ABL
  Status = Handle.QseeComProtocol->QseecomSendCmd (
      Handle.QseeComProtocol, Handle.AppId, (UINT8 *)&FRSUDSReq,
      sizeof (FRSUDSReq), (UINT8 *)&FRSUDSRsp, sizeof (FRSUDSRsp));
#else /*SMCI*/
  size_t RspLenOut = 0;
  Status = IKMHal_sendCmd (AppObj, (CONST VOID *)&FRSUDSReq, sizeof (FRSUDSReq),
                           (VOID *)&FRSUDSRsp, sizeof (FRSUDSRsp), &RspLenOut);
#endif
  if (Status != EFI_SUCCESS ||
      FRSUDSRsp.Status != 0) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterGetFRSAndUDS: Get FRSAndUDS err, "
            "Status: %r, response status: %d\n",
            Status, FRSUDSRsp.Status));
    Status = EFI_LOAD_ERROR;
    goto out;
  }

  if (FRSUDSRsp.FrsLen != DICE_HIDDEN_SIZE &&
      FRSUDSRsp.UdsLen != DICE_CDI_SIZE) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterGetFRSAndUDS: Invalid Length Received:"
            " FrsLen=%d UdsLen=%d\n",
            FRSUDSRsp.FrsLen, FRSUDSRsp.UdsLen));
    Status = EFI_LOAD_ERROR;
    goto out;
  }
  if (!FRSUDSRsp.Frs &&
      !FRSUDSRsp.Uds) {
    DEBUG ((EFI_D_ERROR, "KeyMasterGetFRSAndUDS: Response not received\n"));
    Status = EFI_LOAD_ERROR;
    goto out;
  }

  /* Reset the FDR flag in DevInfo */
  if (Devinfo->FdrFlag == 1) {
    Devinfo->FdrFlag = 0;
  }

  /* Copy FRS secret on devinfo populated by Keymint in response.*/
  Devinfo->FrsSecLen = FRSUDSRsp.FrsSecLen;
  CopyMem (Devinfo->FrsSec, FRSUDSRsp.FrsSec, FRSUDSRsp.FrsSecLen);

  /* Write in devinfo, only when secret is updated or FDR flag is set*/
  Status =
      ReadWriteDeviceInfo (WRITE_CONFIG, (VOID *)Devinfo, sizeof (DeviceInfo));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "KeyMasterGetFRSAndUDS: Unable to Write in"
            " Device Info: %r\n",
            Status));
    goto out;
  }

  CopyMem (bcc_params->FRS, FRSUDSRsp.Frs, FRSUDSRsp.FrsLen);
  CopyMem (bcc_params->UDS, FRSUDSRsp.Uds, FRSUDSRsp.UdsLen);

out:
  if (Devinfo) {
    avb_free (Devinfo);
    Devinfo = NULL;
  }
  return Status;
}
