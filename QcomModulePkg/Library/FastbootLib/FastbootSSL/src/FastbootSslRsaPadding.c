/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2005.
 */
/* ====================================================================
 * Copyright (c) 2005 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "../inc/FastbootSsl.h"
#include <openssl/rsa.h>

#include <openssl/bn.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/rsa/internal.h"
#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/service_indicator/internal.h"
#include "../../../OpenDice/boringssl/src/crypto/internal.h"


#define RSA_PKCS1_PADDING_SIZE 11

int RSA_padding_check_PKCS1_type_1(uint8_t *out, size_t *out_len,
                                   size_t max_out, const uint8_t *from,
                                   size_t from_len) {
  // See RFC 8017, section 9.2. This is part of signature verification and thus
  // does not need to run in constant-time.
  if (from_len < 2) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_SMALL);
    return 0;
  }

  // Check the header.
  if (from[0] != 0 || from[1] != 1) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BLOCK_TYPE_IS_NOT_01);
    return 0;
  }

  // Scan over padded data, looking for the 00.
  size_t pad;
  for (pad = 2 /* header */; pad < from_len; pad++) {
    if (from[pad] == 0x00) {
      break;
    }

    if (from[pad] != 0xff) {
      OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_FIXED_HEADER_DECRYPT);
      return 0;
    }
  }

  if (pad == from_len) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_NULL_BEFORE_BLOCK_MISSING);
    return 0;
  }

  if (pad < 2 /* header */ + 8) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_PAD_BYTE_COUNT);
    return 0;
  }

  // Skip over the 00.
  pad++;

  if (from_len - pad > max_out) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE);
    return 0;
  }

  OPENSSL_memcpy(out, from + pad, from_len - pad);
  *out_len = from_len - pad;
  return 1;
}
