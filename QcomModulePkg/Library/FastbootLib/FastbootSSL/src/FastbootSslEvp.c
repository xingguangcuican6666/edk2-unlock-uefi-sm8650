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
#include "../../../OpenDice/boringssl/src/crypto/internal.h"
#include "../../../OpenDice/boringssl/src/crypto/evp/internal.h"
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/evp.h>
#include <openssl/bytestring.h>
#include <openssl/base64.h>

static const EVP_PKEY_ASN1_METHOD *const kASN1Methods[] = {
    &rsa_asn1_meth,
#ifndef FASTBOOT_SSL_COMPAT
    &ec_asn1_meth,
    &dsa_asn1_meth,
    &ed25519_asn1_meth,
    &x25519_asn1_meth,
#endif
};

EVP_PKEY *EVP_PKEY_new(void) {
  EVP_PKEY *ret;

  ret = OPENSSL_malloc(sizeof(EVP_PKEY));
  if (ret == NULL) {
    return NULL;
  }

  OPENSSL_memset(ret, 0, sizeof(EVP_PKEY));
  ret->type = EVP_PKEY_NONE;
  ret->references = 1;

  return ret;
}

static void free_it(EVP_PKEY *pkey) {
  if (pkey->ameth && pkey->ameth->pkey_free) {
    pkey->ameth->pkey_free(pkey);
    pkey->pkey = NULL;
    pkey->type = EVP_PKEY_NONE;
  }
}

void EVP_PKEY_free(EVP_PKEY *pkey) {
  if (pkey == NULL) {
    return;
  }

  if (!CRYPTO_refcount_dec_and_test_zero(&pkey->references)) {
    return;
  }

  free_it(pkey);
  OPENSSL_free(pkey);
}

int EVP_PKEY_assign(EVP_PKEY *pkey, int type, void *key) {
  if (!EVP_PKEY_set_type(pkey, type)) {
    return 0;
  }
  pkey->pkey = key;
  return key != NULL;
}

int EVP_PKEY_size(const EVP_PKEY *pkey) {
  if (pkey && pkey->ameth && pkey->ameth->pkey_size) {
    return pkey->ameth->pkey_size(pkey);
  }
  return 0;
}

int EVP_PKEY_assign_RSA(EVP_PKEY *pkey, RSA *key) {
  return EVP_PKEY_assign(pkey, EVP_PKEY_RSA, key);
}

static const EVP_PKEY_ASN1_METHOD *evp_pkey_asn1_find(int nid) {
  switch (nid) {
    case EVP_PKEY_RSA:
      return &rsa_asn1_meth;
#ifndef FASTBOOT_SSL_COMPAT
    case EVP_PKEY_EC:
      return &ec_asn1_meth;
    case EVP_PKEY_DSA:
      return &dsa_asn1_meth;
    case EVP_PKEY_ED25519:
      return &ed25519_asn1_meth;
    case EVP_PKEY_X25519:
      return &x25519_asn1_meth;
#endif
    default:
      return NULL;
  }
}

int EVP_PKEY_set_type(EVP_PKEY *pkey, int type) {
  const EVP_PKEY_ASN1_METHOD *ameth;

  if (pkey && pkey->pkey) {
    free_it(pkey);
  }

  ameth = evp_pkey_asn1_find(type);
  if (ameth == NULL) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    ERR_add_error_dataf("algorithm %d", type);
    return 0;
  }

  if (pkey) {
    pkey->ameth = ameth;
    pkey->type = pkey->ameth->pkey_id;
  }

  return 1;
}

static int parse_key_type(CBS *cbs, int *out_type) {
  CBS oid;
  if (!CBS_get_asn1(cbs, &oid, CBS_ASN1_OBJECT)) {
    return 0;
  }

  for (unsigned i = 0; i < OPENSSL_ARRAY_SIZE(kASN1Methods); i++) {
    const EVP_PKEY_ASN1_METHOD *method = kASN1Methods[i];
    if (CBS_len(&oid) == method->oid_len &&
        OPENSSL_memcmp(CBS_data(&oid), method->oid, method->oid_len) == 0) {
      *out_type = method->pkey_id;
      return 1;
    }
  }

  return 0;
}

EVP_PKEY *EVP_parse_public_key(CBS *cbs) {
  // Parse the SubjectPublicKeyInfo.
  CBS spki, algorithm, key;
  int type;
  uint8_t padding;
  if (!CBS_get_asn1(cbs, &spki, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&spki, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&spki, &key, CBS_ASN1_BITSTRING) ||
      CBS_len(&spki) != 0) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return NULL;
  }
  if (!parse_key_type(&algorithm, &type)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    return NULL;
  }
  if (// Every key type defined encodes the key as a byte string with the same
      // conversion to BIT STRING.
      !CBS_get_u8(&key, &padding) ||
      padding != 0) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return NULL;
  }

  // Set up an |EVP_PKEY| of the appropriate type.
  EVP_PKEY *ret = EVP_PKEY_new();
  if (ret == NULL ||
      !EVP_PKEY_set_type(ret, type)) {
    goto err;
  }

  // Call into the type-specific SPKI decoding function.
  if (ret->ameth->pub_decode == NULL) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    goto err;
  }
  if (!ret->ameth->pub_decode(ret, &algorithm, &key)) {
    goto err;
  }

  return ret;

err:
  EVP_PKEY_free(ret);
  return NULL;
}

RSA *EVP_PKEY_get0_RSA(const EVP_PKEY *pkey) {
  if (pkey->type != EVP_PKEY_RSA) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_EXPECTING_AN_RSA_KEY);
    return NULL;
  }
  return pkey->pkey;
}

RSA *EVP_PKEY_get1_RSA(const EVP_PKEY *pkey) {
  RSA *rsa = EVP_PKEY_get0_RSA(pkey);
  if (rsa != NULL) {
    RSA_up_ref(rsa);
  }
  return rsa;
}

RSA *d2i_RSA_PUBKEY(RSA **out, const uint8_t **inp, long len) {
  if (len < 0) {
    return NULL;
  }
  CBS cbs;
  CBS_init(&cbs, *inp, (size_t)len);
  EVP_PKEY *pkey = EVP_parse_public_key(&cbs);
  if (pkey == NULL) {
    return NULL;
  }
  RSA *rsa = EVP_PKEY_get1_RSA(pkey);
  EVP_PKEY_free(pkey);
  if (rsa == NULL) {
    return NULL;
  }
  if (out != NULL) {
    RSA_free(*out);
    *out = rsa;
  }
  *inp = CBS_data(&cbs);
  return rsa;
}
