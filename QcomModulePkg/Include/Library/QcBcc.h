/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __BCC_H__
#define __BCC_H__

typedef INT8 int8_t;
typedef INT16 int16_t;
typedef INT32 int32_t;
typedef INT64 int64_t;
typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
typedef UINTN uintptr_t;
typedef UINTN size_t;
typedef BOOLEAN bool;

#include <dice/dice.h>
/*
 * Size of BCC artifacts handed over from root (with Bcc) is:
 * CBOR tags + memory needed to encode "struct BCCArtifacts"
 *
 *    Size will be : 583 Bytes (2 * DICE_CDI_SIZE + 7 CBOR tags + BCC)
 *
 * However we can allocate little bigger buffer to be future compatible.
 */
#define BCC_ARTIFACTS_WITH_BCC_TOTAL_SIZE                 1024

/* Max Supported BCC Component Version String */
#define BCC_COMPONENT_NAME_BUFFER_MAX_SIZE                32

/* Structure that holds details about the image and it's parameters
 * which may be included in the BCC generation
 */
typedef struct BccImgParams
{
    /* Code Hash of the image which may be included in the BCC data */
    UINT8               codeHash[DICE_HASH_SIZE];

    /* Authority Hash of the image which may be included in the BCC data */
    UINT8               authorityHash[DICE_HASH_SIZE];

    /* Name of the image which may be included in the BCC data */
    CHAR8                  componentName[BCC_COMPONENT_NAME_BUFFER_MAX_SIZE];

    /* Version of the image which may be included in the BCC data */
    UINT64              component_version;
} BccImgParams_t;

/* Structure that is used to pass BCC parameters which may included in BCC */
typedef struct BccParams
{
    /* Unique Device Secret that will be used to derive BCC Root Key */
    UINT8               UDS[DICE_CDI_SIZE];

    /* Factory Reset Sequence that will be included in BCC */
    UINT8               FRS[DICE_HIDDEN_SIZE];

    /* Current mode in which the device is running */
    DiceMode            Mode;

    /* Parameters of the images that may be included in the BCC */
    BccImgParams_t      ChildImage;
} BccParams_t;

/**
  * Function returns the BCC artifacts in the handover format.
  *
  *      BCC artifacts to be handed over from root to the child nodes
  *      takes the following format.
  *
  *         BccHandover = {
  *                        1 : bstr .size 32, // CDI_Attest
  *                        2 : bstr .size 32, // CDI_Seal
  *                        3 : Bcc,           // Cert_Chain
  *                       }
  *         where Bcc = [
  *                       PubKeyEd25519 / PubKeyECDSA256, // Root pub key
  *                       BccEntry,                       // Root -> leaf
  *                     ]
  *
  *      On Success this API will generate and return the BCC Artifacts
  *      in the "FinalEncodedBccArtifacts" (allocated by the calling
  *      client for "BccArtifactsBufferSize") and the actual encoded
  *      BCC Handover artifacts size in "BccArtifactsValidSize".
  *
  * Parameters:
  *
  *    [IN/OUT] FinalEncodedBccArtifacts - BCC encoder buffer to be used
  *                                        to encode BCC Artifacts in
  *                                        BCCHandOver format. This is
  *                                        where the BCC Artifacts will
  *                                        be returned
  *
  *    [IN] BccArtifactsBufferSize       - Buffer size to be used to
  *                                        encode BCC Artifacts in
  *                                        BCCHandOver format
  *
  *    [IN] BccArtifactsValidSize        - Actual size of the final
  *                                        encode BCC Artifacts returned
  *                                        in the BCC encoder buffer
  *
  * Returns:
  *
  *    kDiceResultOk                       - On Success
  *    Appropriate 'DiceResult' error code - On Failure
  *
  */
DiceResult GetBccArtifacts (UINT8   *FinalEncodedBccArtifacts,
                           size_t   BccArtifactsBufferSize,
                           size_t  *BccArtifactsValidSize
#ifndef USE_DUMMY_BCC
                           , BccParams_t BccParamsRecvdFromAVB
#endif
);
#endif
