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
#include <openssl/digest.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/ex_data.h>
#include <openssl/md5.h>
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/sha.h>
#include <openssl/thread.h>

#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/bn/internal.h"
#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/delocate.h"
#include "../../../OpenDice/boringssl/src/crypto/internal.h"
#include "../../../OpenDice/boringssl/src/crypto/fipsmodule/rsa/internal.h"


// RSA_R_BLOCK_TYPE_IS_NOT_02 is part of the legacy SSLv23 padding scheme.
// Cryptography.io depends on this error code.
OPENSSL_DECLARE_ERROR_REASON(RSA, BLOCK_TYPE_IS_NOT_02)

DEFINE_STATIC_EX_DATA_CLASS(g_rsa_ex_data_class)

RSA *RSA_new(void) { return RSA_new_method(NULL); }

RSA *RSA_new_method(const ENGINE *engine) {
  RSA *rsa = OPENSSL_malloc(sizeof(RSA));
  if (rsa == NULL) {
    return NULL;
  }

  OPENSSL_memset(rsa, 0, sizeof(RSA));

  if (engine) {
    rsa->meth = ENGINE_get_RSA_method(engine);
  }

  if (rsa->meth == NULL) {
    rsa->meth = (RSA_METHOD *) RSA_default_method();
  }
  METHOD_ref(rsa->meth);

  rsa->references = 1;
  rsa->flags = rsa->meth->flags;
  CRYPTO_MUTEX_init(&rsa->lock);
  CRYPTO_new_ex_data(&rsa->ex_data);

  if (rsa->meth->init && !rsa->meth->init(rsa)) {
    CRYPTO_free_ex_data(g_rsa_ex_data_class_bss_get(), rsa, &rsa->ex_data);
    CRYPTO_MUTEX_cleanup(&rsa->lock);
    METHOD_unref(rsa->meth);
    OPENSSL_free(rsa);
    return NULL;
  }

  return rsa;
}

void RSA_free(RSA *rsa) {
  unsigned u;

  if (rsa == NULL) {
    return;
  }

  if (!CRYPTO_refcount_dec_and_test_zero(&rsa->references)) {
    return;
  }

  if (rsa->meth->finish) {
    rsa->meth->finish(rsa);
  }
  METHOD_unref(rsa->meth);

  CRYPTO_free_ex_data(g_rsa_ex_data_class_bss_get(), rsa, &rsa->ex_data);

  BN_free(rsa->n);
  BN_free(rsa->e);
  BN_free(rsa->d);
  BN_free(rsa->p);
  BN_free(rsa->q);
  BN_free(rsa->dmp1);
  BN_free(rsa->dmq1);
  BN_free(rsa->iqmp);
  BN_MONT_CTX_free(rsa->mont_n);
  BN_MONT_CTX_free(rsa->mont_p);
  BN_MONT_CTX_free(rsa->mont_q);
  BN_free(rsa->d_fixed);
  BN_free(rsa->dmp1_fixed);
  BN_free(rsa->dmq1_fixed);
  BN_free(rsa->inv_small_mod_large_mont);
  for (u = 0; u < rsa->num_blindings; u++) {
    BN_BLINDING_free(rsa->blindings[u]);
  }
  OPENSSL_free(rsa->blindings);
  OPENSSL_free(rsa->blindings_inuse);
  CRYPTO_MUTEX_cleanup(&rsa->lock);
  OPENSSL_free(rsa);
}

int RSA_up_ref(RSA *rsa) {
  CRYPTO_refcount_inc(&rsa->references);
  return 1;
}

unsigned RSA_bits(const RSA *rsa) { return BN_num_bits(rsa->n); }

const BIGNUM *RSA_get0_n(const RSA *rsa) { return rsa->n; }

const BIGNUM *RSA_get0_e(const RSA *rsa) { return rsa->e; }

const BIGNUM *RSA_get0_d(const RSA *rsa) { return rsa->d; }

const BIGNUM *RSA_get0_p(const RSA *rsa) { return rsa->p; }

const BIGNUM *RSA_get0_q(const RSA *rsa) { return rsa->q; }

const BIGNUM *RSA_get0_dmp1(const RSA *rsa) { return rsa->dmp1; }

const BIGNUM *RSA_get0_dmq1(const RSA *rsa) { return rsa->dmq1; }

const BIGNUM *RSA_get0_iqmp(const RSA *rsa) { return rsa->iqmp; }

void RSA_get0_key(const RSA *rsa, const BIGNUM **out_n, const BIGNUM **out_e,
                  const BIGNUM **out_d) {
  if (out_n != NULL) {
    *out_n = rsa->n;
  }
  if (out_e != NULL) {
    *out_e = rsa->e;
  }
  if (out_d != NULL) {
    *out_d = rsa->d;
  }
}

void RSA_get0_factors(const RSA *rsa, const BIGNUM **out_p,
                      const BIGNUM **out_q) {
  if (out_p != NULL) {
    *out_p = rsa->p;
  }
  if (out_q != NULL) {
    *out_q = rsa->q;
  }
}

const RSA_PSS_PARAMS *RSA_get0_pss_params(const RSA *rsa) {
  // We do not support the id-RSASSA-PSS key encoding. If we add support later,
  // the |maskHash| field should be filled in for OpenSSL compatibility.
  return NULL;
}

void RSA_get0_crt_params(const RSA *rsa, const BIGNUM **out_dmp1,
                         const BIGNUM **out_dmq1, const BIGNUM **out_iqmp) {
  if (out_dmp1 != NULL) {
    *out_dmp1 = rsa->dmp1;
  }
  if (out_dmq1 != NULL) {
    *out_dmq1 = rsa->dmq1;
  }
  if (out_iqmp != NULL) {
    *out_iqmp = rsa->iqmp;
  }
}

int RSA_set0_key(RSA *rsa, BIGNUM *n, BIGNUM *e, BIGNUM *d) {
  if ((rsa->n == NULL && n == NULL) ||
      (rsa->e == NULL && e == NULL)) {
    return 0;
  }

  if (n != NULL) {
    BN_free(rsa->n);
    rsa->n = n;
  }
  if (e != NULL) {
    BN_free(rsa->e);
    rsa->e = e;
  }
  if (d != NULL) {
    BN_free(rsa->d);
    rsa->d = d;
  }

  return 1;
}

int RSA_set0_factors(RSA *rsa, BIGNUM *p, BIGNUM *q) {
  if ((rsa->p == NULL && p == NULL) ||
      (rsa->q == NULL && q == NULL)) {
    return 0;
  }

  if (p != NULL) {
    BN_free(rsa->p);
    rsa->p = p;
  }
  if (q != NULL) {
    BN_free(rsa->q);
    rsa->q = q;
  }

  return 1;
}

int RSA_set0_crt_params(RSA *rsa, BIGNUM *dmp1, BIGNUM *dmq1, BIGNUM *iqmp) {
  if ((rsa->dmp1 == NULL && dmp1 == NULL) ||
      (rsa->dmq1 == NULL && dmq1 == NULL) ||
      (rsa->iqmp == NULL && iqmp == NULL)) {
    return 0;
  }

  if (dmp1 != NULL) {
    BN_free(rsa->dmp1);
    rsa->dmp1 = dmp1;
  }
  if (dmq1 != NULL) {
    BN_free(rsa->dmq1);
    rsa->dmq1 = dmq1;
  }
  if (iqmp != NULL) {
    BN_free(rsa->iqmp);
    rsa->iqmp = iqmp;
  }

  return 1;
}

int RSA_decrypt(RSA *rsa, size_t *out_len, uint8_t *out, size_t max_out,
                const uint8_t *in, size_t in_len, int padding) {
  if (rsa->meth->decrypt) {
    return rsa->meth->decrypt(rsa, out_len, out, max_out, in, in_len, padding);
  }

  return rsa_default_decrypt(rsa, out_len, out, max_out, in, in_len, padding);
}

int RSA_public_decrypt(size_t flen, const uint8_t *from, uint8_t *to, RSA *rsa,
                       int padding) {
  size_t out_len;
  if (!RSA_verify_raw(rsa, &out_len, to, RSA_size(rsa), from, flen, padding)) {
    return -1;
  }

  if (out_len > INT_MAX) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_OVERFLOW);
    return -1;
  }
  return (int)out_len;
}

unsigned RSA_size(const RSA *rsa) {
  size_t ret = rsa->meth->size ? rsa->meth->size(rsa) : rsa_default_size(rsa);
  // RSA modulus sizes are bounded by |BIGNUM|, which must fit in |unsigned|.
  //
  // TODO(https://crbug.com/boringssl/516): Should we make this return |size_t|?
  assert(ret < UINT_MAX);
  return (unsigned)ret;
}

int RSA_is_opaque(const RSA *rsa) {
  return rsa->meth && (rsa->meth->flags & RSA_FLAG_OPAQUE);
}

static int check_mod_inverse(int *out_ok, const BIGNUM *a, const BIGNUM *ainv,
                             const BIGNUM *m, unsigned m_min_bits,
                             BN_CTX *ctx) {
  if (BN_is_negative(ainv) || BN_cmp(ainv, m) >= 0) {
    *out_ok = 0;
    return 1;
  }

  // Note |bn_mul_consttime| and |bn_div_consttime| do not scale linearly, but
  // checking |ainv| is in range bounds the running time, assuming |m|'s bounds
  // were checked by the caller.
  BN_CTX_start(ctx);
  BIGNUM *tmp = BN_CTX_get(ctx);
  int ret = tmp != NULL &&
            bn_mul_consttime(tmp, a, ainv, ctx) &&
            bn_div_consttime(NULL, tmp, tmp, m, m_min_bits, ctx);
  if (ret) {
    *out_ok = BN_is_one(tmp);
  }
  BN_CTX_end(ctx);
  return ret;
}

int RSA_check_key(const RSA *key) {
  // TODO(davidben): RSA key initialization is spread across
  // |rsa_check_public_key|, |RSA_check_key|, |freeze_private_key|, and
  // |BN_MONT_CTX_set_locked| as a result of API issues. See
  // https://crbug.com/boringssl/316. As a result, we inconsistently check RSA
  // invariants. We should fix this and integrate that logic.

  if (RSA_is_opaque(key)) {
    // Opaque keys can't be checked.
    return 1;
  }

  if (!rsa_check_public_key(key)) {
    return 0;
  }

  if ((key->p != NULL) != (key->q != NULL)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_ONLY_ONE_OF_P_Q_GIVEN);
    return 0;
  }

  // |key->d| must be bounded by |key->n|. This ensures bounds on |RSA_bits|
  // translate to bounds on the running time of private key operations.
  if (key->d != NULL &&
      (BN_is_negative(key->d) || BN_cmp(key->d, key->n) >= 0)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_D_OUT_OF_RANGE);
    return 0;
  }

  if (key->d == NULL || key->p == NULL) {
    // For a public key, or without p and q, there's nothing that can be
    // checked.
    return 1;
  }

  BN_CTX *ctx = BN_CTX_new();
  if (ctx == NULL) {
    return 0;
  }

  BIGNUM tmp, de, pm1, qm1, dmp1, dmq1;
  int ok = 0;
  BN_init(&tmp);
  BN_init(&de);
  BN_init(&pm1);
  BN_init(&qm1);
  BN_init(&dmp1);
  BN_init(&dmq1);

  // Check that p * q == n. Before we multiply, we check that p and q are in
  // bounds, to avoid a DoS vector in |bn_mul_consttime| below. Note that
  // n was bound by |rsa_check_public_key|. This also implicitly checks p and q
  // are odd, which is a necessary condition for Montgomery reduction.
  if (BN_is_negative(key->p) || BN_cmp(key->p, key->n) >= 0 ||
      BN_is_negative(key->q) || BN_cmp(key->q, key->n) >= 0) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_N_NOT_EQUAL_P_Q);
    goto out;
  }
  if (!bn_mul_consttime(&tmp, key->p, key->q, ctx)) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    goto out;
  }
  if (BN_cmp(&tmp, key->n) != 0) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_N_NOT_EQUAL_P_Q);
    goto out;
  }

  // d must be an inverse of e mod the Carmichael totient, lcm(p-1, q-1), but it
  // may be unreduced because other implementations use the Euler totient. We
  // simply check that d * e is one mod p-1 and mod q-1. Note d and e were bound
  // by earlier checks in this function.
  if (!bn_usub_consttime(&pm1, key->p, BN_value_one()) ||
      !bn_usub_consttime(&qm1, key->q, BN_value_one())) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    goto out;
  }
  const unsigned pm1_bits = BN_num_bits(&pm1);
  const unsigned qm1_bits = BN_num_bits(&qm1);
  if (!bn_mul_consttime(&de, key->d, key->e, ctx) ||
      !bn_div_consttime(NULL, &tmp, &de, &pm1, pm1_bits, ctx) ||
      !bn_div_consttime(NULL, &de, &de, &qm1, qm1_bits, ctx)) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    goto out;
  }

  if (!BN_is_one(&tmp) || !BN_is_one(&de)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_D_E_NOT_CONGRUENT_TO_1);
    goto out;
  }

  int has_crt_values = key->dmp1 != NULL;
  if (has_crt_values != (key->dmq1 != NULL) ||
      has_crt_values != (key->iqmp != NULL)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_INCONSISTENT_SET_OF_CRT_VALUES);
    goto out;
  }

  if (has_crt_values) {
    int dmp1_ok, dmq1_ok, iqmp_ok;
    if (!check_mod_inverse(&dmp1_ok, key->e, key->dmp1, &pm1, pm1_bits, ctx) ||
        !check_mod_inverse(&dmq1_ok, key->e, key->dmq1, &qm1, qm1_bits, ctx) ||
        // |p| is odd, so |pm1| and |p| have the same bit width. If they didn't,
        // we only need a lower bound anyway.
        !check_mod_inverse(&iqmp_ok, key->q, key->iqmp, key->p, pm1_bits,
                           ctx)) {
      OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
      goto out;
    }

    if (!dmp1_ok || !dmq1_ok || !iqmp_ok) {
      OPENSSL_PUT_ERROR(RSA, RSA_R_CRT_VALUES_INCORRECT);
      goto out;
    }
  }

  ok = 1;

out:
  BN_free(&tmp);
  BN_free(&de);
  BN_free(&pm1);
  BN_free(&qm1);
  BN_free(&dmp1);
  BN_free(&dmq1);
  BN_CTX_free(ctx);

  return ok;
}

