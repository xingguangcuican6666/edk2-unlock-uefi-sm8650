/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "opendice-util.h"
#include "QcBcc.h"

#include <LinuxLoaderLib.h>
#include <dice/android/bcc.h>
#include <dice/cbor_writer.h>
#include <dice/ops.h>
#include <dice/ops/trait/cose.h>
#include <dice/utils.h>

/* Max size of COSE_Sign1 including payload. */
#define BCC_MAX_CERTIFICATE_SIZE               512

/*
 * Size of a BCC artifacts handed over from root (without Bcc) is:
 * CBOR tags + Two CDIs = 71
 */
#define BCC_ARTIFACTS_WO_BCC_TOTAL_SIZE        71

/* Actual Size of BCC Configuration Descriptor field */
#define BCC_CONFIG_DESCRIPTOR_TOTAL_SIZE       48

/* Set of information required to derive DICE artifacts for the child node. */
typedef struct BccChildParams
{
    UINT8 codeHash[DICE_HASH_SIZE];       /* Code Hash */
    UINT8 authorityHash[DICE_HASH_SIZE];  /* Authority Hash */
    BccConfigValues bccCfgDesc;           /* Bcc Config Descriptor */
} BccChildParams_t;

typedef struct BccRootState
{
    /* Unique Device Secret */
    UINT8 UDS[DICE_CDI_SIZE];                /* Unique Device Secret */
    /* Public key of the key pair derived from a seed derived from UDS */
    UINT8 UDSPubKey[DICE_PUBLIC_KEY_SIZE];
    /* Secret with factory reset life time */
    UINT8 FRS[DICE_HIDDEN_SIZE];
    /* Device Mode */
    DiceMode Mode;
    /* Parameters of next stage/child Image */
    BccChildParams_t ChildImage;
} BccRoot_t;

/* Set of BCC artifacts (BCC Handover Format) passed on from one stage
   to the next */
typedef struct BCCArtifacts
{
    UINT8               nextCDIAttest[DICE_CDI_SIZE];
    UINT8               nextCDISeal[DICE_CDI_SIZE];
    UINT8               nextBCC[BCC_MAX_CERTIFICATE_SIZE];
    size_t              nextBCCSize;
} BCCArtifacts_t;

STATIC CONST INT64 kCdiAttestLabel = 1;
STATIC CONST INT64 kCdiSealLabel   = 2;

/* Data structure that holds details of root node and the parameters of
   the images that will be used to generated the final encoded BCC Artifacts */
static BccRoot_t     BccRoot;

/* Function that returns BCC artifacts in the handover format.*/
DiceResult GetBccArtifacts (UINT8    *FinalEncodedBccArtifacts,
                            size_t    BccArtifactsBufferSize,
                            size_t   *BccArtifactsValidSize
#ifndef USE_DUMMY_BCC
                            , BccParams_t    BccParamsRecvdFromAVB
#endif
)
{
    UINT8 UDSPrivateKeySeed[DICE_PRIVATE_KEY_SEED_SIZE] = {0};
    UINT8 UDSPrivateKey[DICE_PRIVATE_KEY_SIZE] = {0};
    UINT8 BccEncodedConfigDesc[BCC_CONFIG_DESCRIPTOR_TOTAL_SIZE] = {0};
    UINT8 NextBccEncodedCDIs[BCC_ARTIFACTS_WO_BCC_TOTAL_SIZE] = {0};
    size_t  BccEncodedConfigDescValidSize = 0;
    size_t  NextBccEncodedCDIsValidSize = 0;
    BCCArtifacts_t  bccCDIsOnly = {{0}};
    DiceInputValues BccInputValues = {{0}};
    struct  CborOut Out;
    DiceResult Result;

#ifdef USE_DUMMY_BCC
    // Fill some hard code values here for now. AVB team has to provide
    // the real values.
    BccParams_t     BccParamsRecvdFromAVB = {{0}};
    memcpy ((void *)BccParamsRecvdFromAVB.ChildImage.componentName,
                                                                  "pvmfw", 5);
#endif

    assert (FinalEncodedBccArtifacts);
    assert (BccArtifactsValidSize);
    assert (BccArtifactsBufferSize >= BCC_ARTIFACTS_WITH_BCC_TOTAL_SIZE);

    //---------------------------------------------------------------------
    // Populate BCC Root Data Structure with parameters received from AVB
    //---------------------------------------------------------------------

    // Clear BCC Root State datastructure
    SetMem (&BccRoot, sizeof (BccRoot), 0);

    // Copy UDS (Unique Device Secret) received from AVB
    memcpy (BccRoot.UDS,
           BccParamsRecvdFromAVB.UDS,
           DICE_CDI_SIZE);

    // Copy FRS (Factory Reset Sequence) received from AVB
    memcpy (BccRoot.FRS,
           BccParamsRecvdFromAVB.FRS,
           DICE_HIDDEN_SIZE);

    // Copy code hash received from AVB
    memcpy (BccRoot.ChildImage.codeHash,
           BccParamsRecvdFromAVB.ChildImage.codeHash,
           DICE_HIDDEN_SIZE);

    // Copy authority hash received from AVB
    memcpy (BccRoot.ChildImage.authorityHash,
           BccParamsRecvdFromAVB.ChildImage.authorityHash,
           DICE_HIDDEN_SIZE);

    // Update image component name
    BccRoot.ChildImage.bccCfgDesc.component_name =
                    BccParamsRecvdFromAVB.ChildImage.componentName;

    // Copy image component version received from AVB
    BccRoot.ChildImage.bccCfgDesc.component_version =
                    BccParamsRecvdFromAVB.ChildImage.component_version;

    // Copy device mode received from AVB
    BccRoot.Mode = BccParamsRecvdFromAVB.Mode;

    // Finally select Config Descriptor fields to include in BCC input
    BccRoot.ChildImage.bccCfgDesc.inputs =
                    BCC_INPUT_COMPONENT_NAME | BCC_INPUT_COMPONENT_VERSION;

    //---------------------------------------------------------------------
    //                  Derive Private Key Seed from UDS
    //---------------------------------------------------------------------
    Result = DiceDeriveCdiPrivateKeySeed (NULL, BccRoot.UDS, UDSPrivateKeySeed);
    if (Result != kDiceResultOk) {
        DEBUG ((EFI_D_ERROR, "Failed to derive a seed for UDS key pair.\n"));
        return Result;
    }

    //---------------------------------------------------------------------
    //                       Derive UDS Key Pair
    //---------------------------------------------------------------------

    /* UDS public key is kept in root to construct the certificate
     * chain for the child nodes. UDS private key is derived in every
     * DICE operation which uses it.
     */
    Result = DiceKeypairFromSeed (NULL, UDSPrivateKeySeed,
                                 BccRoot.UDSPubKey,
                                UDSPrivateKey);
    if (Result != kDiceResultOk) {
      DEBUG ((EFI_D_ERROR, "Failed to derive UDS key pair.\n"));
      return Result;
    }

    //---------------------------------------------------------------------
    //           CBOR Encode BCC Config Descriptor Parameters
    //---------------------------------------------------------------------
    Result = BccFormatConfigDescriptor (&(BccRoot.ChildImage.bccCfgDesc),
                                       sizeof (BccEncodedConfigDesc),
                                       BccEncodedConfigDesc,
                                       &BccEncodedConfigDescValidSize);
    if (Result != kDiceResultOk) {
      DEBUG ((EFI_D_ERROR, "Failed to format config descriptor : %d\n",
                                                                      Result));
      return Result;
    }

    //---------------------------------------------------------------------
    //                  Initialize the DICE input values
    //---------------------------------------------------------------------
    // Initialize code hash
    memcpy (BccInputValues.code_hash,
           BccRoot.ChildImage.codeHash,
           sizeof (BccRoot.ChildImage.codeHash));

    // Initialize authority hash
    memcpy (BccInputValues.authority_hash,
           BccRoot.ChildImage.authorityHash,
           sizeof (BccRoot.ChildImage.authorityHash));

    // Initialize Factory reset secret being used
    memcpy (BccInputValues.hidden,
           BccRoot.FRS,
           sizeof (BccRoot.FRS));

    BccInputValues.config_type               = kDiceConfigTypeDescriptor;
    BccInputValues.config_descriptor         = BccEncodedConfigDesc;
    BccInputValues.config_descriptor_size    = BccEncodedConfigDescValidSize;
    BccInputValues.mode                      = BccRoot.Mode;

    //---------------------------------------------------------------------
    // Generate Dice Artifacts Without BCC (CDI-Attest, CDI-Sealing only)
    //---------------------------------------------------------------------
    Result = DiceMainFlow (NULL,
                           BccRoot.UDS,
                           BccRoot.UDS,
                           &BccInputValues,
                           0,
                          NULL,
                          NULL,
                          bccCDIsOnly.nextCDIAttest,
                          bccCDIsOnly.nextCDISeal);
    if (Result != kDiceResultOk) {
        DEBUG ((EFI_D_ERROR, "Failed to derive DICE CDIs : %d\n", Result));
        return Result;
    }

    //---------------------------------------------------------------------
    // CBOR Encode Dice Artifacts (Without BCC) CDI-Attest/CDI-Sealing
    //---------------------------------------------------------------------
    CborOutInit (NextBccEncodedCDIs, BCC_ARTIFACTS_WO_BCC_TOTAL_SIZE, &Out);

    CborWriteMap (2, &Out);

    CborWriteInt (kCdiAttestLabel, &Out);
    CborWriteBstr (DICE_CDI_SIZE, bccCDIsOnly.nextCDIAttest, &Out);

    CborWriteInt (kCdiSealLabel, &Out);
    CborWriteBstr (DICE_CDI_SIZE, bccCDIsOnly.nextCDISeal, &Out);

    assert (!CborOutOverflowed (&Out));
    NextBccEncodedCDIsValidSize = CborOutSize (&Out);

    //---------------------------------------------------------------------
    // Generate Dice Artifacts With BCC (CDI-Attest, CDI-Sealing, BCC)
    //---------------------------------------------------------------------
    Result = BccHandoverMainFlow (NULL /*context=*/,
                                 NextBccEncodedCDIs,
                                 NextBccEncodedCDIsValidSize,
                                 &BccInputValues,
                                 BccArtifactsBufferSize,
                                 FinalEncodedBccArtifacts,
                                 BccArtifactsValidSize);
    return Result;
}
