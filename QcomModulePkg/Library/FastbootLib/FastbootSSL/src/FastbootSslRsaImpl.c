/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "../inc/FastbootSsl.h"

#include <openssl/rsa.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/thread.h>

#include "../../../OpenDice/boringssl/src/crypto/internal.h"
#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/bn/internal.h"
#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/delocate.h"
#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/rand/fork_detect.h"
#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/service_indicator/internal.h"
#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/rsa/internal.h"

DEFINE_METHOD_FUNCTION(RSA_METHOD, RSA_default_method) {
  // All of the methods are NULL to make it easier for the compiler/linker to
  // drop unused functions. The wrapper functions will select the appropriate
  // |rsa_default_*| implementation.
  OPENSSL_memset(out, 0, sizeof(RSA_METHOD));
  out->common.is_static = 1;
}

size_t rsa_default_size(const RSA *rsa) {
  return BN_num_bytes(rsa->n);
}

int rsa_check_public_key(const RSA *rsa) {
  if (rsa->n == NULL || rsa->e == NULL) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  unsigned n_bits = BN_num_bits(rsa->n);
  if (n_bits > 16 * 1024) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_MODULUS_TOO_LARGE);
    return 0;
  }

  // RSA moduli must be odd. In addition to being necessary for RSA in general,
  // we cannot setup Montgomery reduction with even moduli.
  if (!BN_is_odd(rsa->n)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_RSA_PARAMETERS);
    return 0;
  }

  // Mitigate DoS attacks by limiting the exponent size. 33 bits was chosen as
  // the limit based on the recommendations in [1] and [2]. Windows CryptoAPI
  // doesn't support values larger than 32 bits [3], so it is unlikely that
  // exponents larger than 32 bits are being used for anything Windows commonly
  // does.
  //
  // [1] https://www.imperialviolet.org/2012/03/16/rsae.html
  // [2] https://www.imperialviolet.org/2012/03/17/rsados.html
  // [3] https://msdn.microsoft.com/en-us/library/aa387685(VS.85).aspx
  static const unsigned kMaxExponentBits = 33;
  unsigned e_bits = BN_num_bits(rsa->e);
  if (e_bits > kMaxExponentBits ||
      // Additionally reject e = 1 or even e. e must be odd to be relatively
      // prime with phi(n).
      e_bits < 2 ||
      !BN_is_odd(rsa->e)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
    return 0;
  }

  // Verify |n > e|. Comparing |n_bits| to |kMaxExponentBits| is a small
  // shortcut to comparing |n| and |e| directly. In reality, |kMaxExponentBits|
  // is much smaller than the minimum RSA key size that any application should
  // accept.
  if (n_bits <= kMaxExponentBits) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return 0;
  }
  assert(BN_ucmp(rsa->n, rsa->e) > 0);

  return 1;
}

int rsa_verify_raw_no_self_test(RSA *rsa, size_t *out_len, uint8_t *out,
                                size_t max_out, const uint8_t *in,
                                size_t in_len, int padding) {
  if (!rsa_check_public_key(rsa)) {
    return 0;
  }

  const unsigned rsa_size = RSA_size(rsa);
  BIGNUM *f, *result;

  if (max_out < rsa_size) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_OUTPUT_BUFFER_TOO_SMALL);
    return 0;
  }

  if (in_len != rsa_size) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_LEN_NOT_EQUAL_TO_MOD_LEN);
    return 0;
  }

  BN_CTX *ctx = BN_CTX_new();
  if (ctx == NULL) {
    return 0;
  }

  int ret = 0;
  uint8_t *buf = NULL;

  BN_CTX_start(ctx);
  f = BN_CTX_get(ctx);
  result = BN_CTX_get(ctx);
  if (f == NULL || result == NULL) {
    goto err;
  }

  if (padding == RSA_NO_PADDING) {
    buf = out;
  } else {
    // Allocate a temporary buffer to hold the padded plaintext.
    buf = OPENSSL_malloc(rsa_size);
    if (buf == NULL) {
      goto err;
    }
  }

  if (BN_bin2bn(in, in_len, f) == NULL) {
    goto err;
  }

  if (BN_ucmp(f, rsa->n) >= 0) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
    goto err;
  }

  if (!BN_MONT_CTX_set_locked(&rsa->mont_n, &rsa->lock, rsa->n, ctx) ||
      !BN_mod_exp_mont(result, f, rsa->e, &rsa->mont_n->N, ctx, rsa->mont_n)) {
    goto err;
  }

  if (!BN_bn2bin_padded(buf, rsa_size, result)) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    goto err;
  }

  switch (padding) {
    case RSA_PKCS1_PADDING:
      ret =
          RSA_padding_check_PKCS1_type_1(out, out_len, rsa_size, buf, rsa_size);
      break;
    case RSA_NO_PADDING:
      ret = 1;
      *out_len = rsa_size;
      break;
    default:
      OPENSSL_PUT_ERROR(RSA, RSA_R_UNKNOWN_PADDING_TYPE);
      goto err;
  }

  if (!ret) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_PADDING_CHECK_FAILED);
    goto err;
  }

err:
  BN_CTX_end(ctx);
  BN_CTX_free(ctx);
  if (buf != out) {
    OPENSSL_free(buf);
  }
  return ret;
}

int RSA_verify_raw(RSA *rsa, size_t *out_len, uint8_t *out,
                                size_t max_out, const uint8_t *in,
                                size_t in_len, int padding) {
  boringssl_ensure_rsa_self_test();
  return rsa_verify_raw_no_self_test(rsa, out_len, out, max_out, in, in_len,
                                     padding);
}
