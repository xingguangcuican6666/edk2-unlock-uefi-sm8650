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
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
#include "../../../OpenDice/boringssl/src/crypto/internal.h"
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/buf.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/base64.h>
#include <openssl/cipher.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

IMPLEMENT_PEM_rw(RSA_PUBKEY, RSA, PEM_STRING_PUBLIC, RSA_PUBKEY)

int PEM_read_bio(BIO *bp, char **name, char **header, unsigned char **data,
                 long *len) {
  EVP_ENCODE_CTX ctx;
  int end = 0, i, k, bl = 0, hl = 0, nohead = 0;
  char buf[256];
  BUF_MEM *nameB;
  BUF_MEM *headerB;
  BUF_MEM *dataB, *tmpB;

  nameB = BUF_MEM_new();
  headerB = BUF_MEM_new();
  dataB = BUF_MEM_new();
  if ((nameB == NULL) || (headerB == NULL) || (dataB == NULL)) {
    BUF_MEM_free(nameB);
    BUF_MEM_free(headerB);
    BUF_MEM_free(dataB);
    return 0;
  }

  buf[254] = '\0';
  for (;;) {
    i = BIO_gets(bp, buf, 254);

    if (i <= 0) {
      OPENSSL_PUT_ERROR(PEM, PEM_R_NO_START_LINE);
      goto err;
    }

    while ((i >= 0) && (buf[i] <= ' ')) {
      i--;
    }
    buf[++i] = '\n';
    buf[++i] = '\0';

    if (strncmp(buf, "-----BEGIN ", 11) == 0) {
      i = strlen(&(buf[11]));

      if (strncmp(&(buf[11 + i - 6]), "-----\n", 6) != 0) {
        continue;
      }
      if (!BUF_MEM_grow(nameB, i + 9)) {
        goto err;
      }
      OPENSSL_memcpy(nameB->data, &(buf[11]), i - 6);
      nameB->data[i - 6] = '\0';
      break;
    }
  }
  hl = 0;
  if (!BUF_MEM_grow(headerB, 256)) {
    goto err;
  }
  headerB->data[0] = '\0';
  for (;;) {
    i = BIO_gets(bp, buf, 254);
    if (i <= 0) {
      break;
    }

    while ((i >= 0) && (buf[i] <= ' ')) {
      i--;
    }
    buf[++i] = '\n';
    buf[++i] = '\0';

    if (buf[0] == '\n') {
      break;
    }
    if (!BUF_MEM_grow(headerB, hl + i + 9)) {
      goto err;
    }
    if (strncmp(buf, "-----END ", 9) == 0) {
      nohead = 1;
      break;
    }
    OPENSSL_memcpy(&(headerB->data[hl]), buf, i);
    headerB->data[hl + i] = '\0';
    hl += i;
  }

  bl = 0;
  if (!BUF_MEM_grow(dataB, 1024)) {
    goto err;
  }
  dataB->data[0] = '\0';
  if (!nohead) {
    for (;;) {
      i = BIO_gets(bp, buf, 254);
      if (i <= 0) {
        break;
      }

      while ((i >= 0) && (buf[i] <= ' ')) {
        i--;
      }
      buf[++i] = '\n';
      buf[++i] = '\0';

      if (i != 65) {
        end = 1;
      }
      if (strncmp(buf, "-----END ", 9) == 0) {
        break;
      }
      if (i > 65) {
        break;
      }
      if (!BUF_MEM_grow_clean(dataB, i + bl + 9)) {
        goto err;
      }
      OPENSSL_memcpy(&(dataB->data[bl]), buf, i);
      dataB->data[bl + i] = '\0';
      bl += i;
      if (end) {
        buf[0] = '\0';
        i = BIO_gets(bp, buf, 254);
        if (i <= 0) {
          break;
        }

        while ((i >= 0) && (buf[i] <= ' ')) {
          i--;
        }
        buf[++i] = '\n';
        buf[++i] = '\0';

        break;
      }
    }
  } else {
    tmpB = headerB;
    headerB = dataB;
    dataB = tmpB;
    bl = hl;
  }
  i = strlen(nameB->data);
  if ((strncmp(buf, "-----END ", 9) != 0) ||
      (strncmp(nameB->data, &(buf[9]), i) != 0) ||
      (strncmp(&(buf[9 + i]), "-----\n", 6) != 0)) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_END_LINE);
    goto err;
  }

  EVP_DecodeInit(&ctx);
  i = EVP_DecodeUpdate(&ctx, (unsigned char *)dataB->data, &bl,
                       (unsigned char *)dataB->data, bl);
  if (i < 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_BASE64_DECODE);
    goto err;
  }
  i = EVP_DecodeFinal(&ctx, (unsigned char *)&(dataB->data[bl]), &k);
  if (i < 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_BASE64_DECODE);
    goto err;
  }
  bl += k;

  if (bl == 0) {
    goto err;
  }
  *name = nameB->data;
  *header = headerB->data;
  *data = (unsigned char *)dataB->data;
  *len = bl;
  OPENSSL_free(nameB);
  OPENSSL_free(headerB);
  OPENSSL_free(dataB);
  return 1;
err:
  BUF_MEM_free(nameB);
  BUF_MEM_free(headerB);
  BUF_MEM_free(dataB);
  return 0;
}

static int check_pem(const char *nm, const char *name) {
  // Normal matching nm and name
  if (!strcmp(nm, name)) {
    return 1;
  }

  // Make PEM_STRING_EVP_PKEY match any private key

  if (!strcmp(name, PEM_STRING_EVP_PKEY)) {
    return !strcmp(nm, PEM_STRING_PKCS8) || !strcmp(nm, PEM_STRING_PKCS8INF) ||
           !strcmp(nm, PEM_STRING_RSA) || !strcmp(nm, PEM_STRING_EC) ||
           !strcmp(nm, PEM_STRING_DSA);
  }

  // Permit older strings

  if (!strcmp(nm, PEM_STRING_X509_OLD) && !strcmp(name, PEM_STRING_X509)) {
    return 1;
  }

  if (!strcmp(nm, PEM_STRING_X509_REQ_OLD) &&
      !strcmp(name, PEM_STRING_X509_REQ)) {
    return 1;
  }

  // Allow normal certs to be read as trusted certs
  if (!strcmp(nm, PEM_STRING_X509) && !strcmp(name, PEM_STRING_X509_TRUSTED)) {
    return 1;
  }

  if (!strcmp(nm, PEM_STRING_X509_OLD) &&
      !strcmp(name, PEM_STRING_X509_TRUSTED)) {
    return 1;
  }

  // Some CAs use PKCS#7 with CERTIFICATE headers
  if (!strcmp(nm, PEM_STRING_X509) && !strcmp(name, PEM_STRING_PKCS7)) {
    return 1;
  }

  if (!strcmp(nm, PEM_STRING_PKCS7_SIGNED) && !strcmp(name, PEM_STRING_PKCS7)) {
    return 1;
  }

#ifndef OPENSSL_NO_CMS
  if (!strcmp(nm, PEM_STRING_X509) && !strcmp(name, PEM_STRING_CMS)) {
    return 1;
  }
  // Allow CMS to be read from PKCS#7 headers
  if (!strcmp(nm, PEM_STRING_PKCS7) && !strcmp(name, PEM_STRING_CMS)) {
    return 1;
  }
#endif

  return 0;
}

static int load_iv(char **fromp, unsigned char *to, size_t num) {
  uint8_t v;
  char *from;

  from = *fromp;
  for (size_t i = 0; i < num; i++) {
    to[i] = 0;
  }
  num *= 2;
  for (size_t i = 0; i < num; i++) {
    if (!OPENSSL_fromxdigit(&v, *from)) {
      OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_IV_CHARS);
      return 0;
    }
    from++;
    to[i / 2] |= v << (!(i & 1)) * 4;
  }

  *fromp = from;
  return 1;
}

static const EVP_CIPHER *cipher_by_name(const char *name) {
  // This is similar to the (deprecated) function |EVP_get_cipherbyname|. Note
  // the PEM code assumes that ciphers have at least 8 bytes of IV, at most 20
  // bytes of overhead and generally behave like CBC mode.
#ifndef FASTBOOT_SSL_COMPAT
  if (0 == strcmp(name, SN_des_cbc)) {
    return EVP_des_cbc();
  } else if (0 == strcmp(name, SN_des_ede3_cbc)) {
    return EVP_des_ede3_cbc();
  } else if (0 == strcmp(name, SN_aes_128_cbc)) {
    return EVP_aes_128_cbc();
  } else if (0 == strcmp(name, SN_aes_192_cbc)) {
    return EVP_aes_192_cbc();
  } else if (0 == strcmp(name, SN_aes_256_cbc)) {
    return EVP_aes_256_cbc();
  } else {
    return NULL;
  }
#else
  DEBUG((EFI_D_ERROR, "cipher_by_name %s\n", name));

  if (0 == strcmp(name, SN_des_cbc)) {
    return NULL;
  } else if (0 == strcmp(name, SN_des_ede3_cbc)) {
    return NULL;
  } else if (0 == strcmp(name, SN_aes_128_cbc)) {
    return NULL;
  } else if (0 == strcmp(name, SN_aes_192_cbc)) {
    return NULL;
  } else if (0 == strcmp(name, SN_aes_256_cbc)) {
    return NULL;
  } else {
    return NULL;
  }
#endif
}

int PEM_get_EVP_CIPHER_INFO(char *header, EVP_CIPHER_INFO *cipher) {
  const EVP_CIPHER *enc = NULL;
  char *p, c;
  char **header_pp = &header;

  cipher->cipher = NULL;
  OPENSSL_memset(cipher->iv, 0, sizeof(cipher->iv));
  if ((header == NULL) || (*header == '\0') || (*header == '\n')) {
    return 1;
  }
  if (strncmp(header, "Proc-Type: ", 11) != 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_NOT_PROC_TYPE);
    return 0;
  }
  header += 11;
  if (*header != '4') {
    return 0;
  }
  header++;
  if (*header != ',') {
    return 0;
  }
  header++;
  if (strncmp(header, "ENCRYPTED", 9) != 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_NOT_ENCRYPTED);
    return 0;
  }
  for (; (*header != '\n') && (*header != '\0'); header++) {
    ;
  }
  if (*header == '\0') {
    OPENSSL_PUT_ERROR(PEM, PEM_R_SHORT_HEADER);
    return 0;
  }
  header++;
  if (strncmp(header, "DEK-Info: ", 10) != 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_NOT_DEK_INFO);
    return 0;
  }
  header += 10;

  p = header;
  for (;;) {
    c = *header;
    if (!((c >= 'A' && c <= 'Z') || c == '-' ||
          OPENSSL_isdigit(c))) {
      break;
    }
    header++;
  }
  *header = '\0';
  cipher->cipher = enc = cipher_by_name(p);
  *header = c;
  header++;

  if (enc == NULL) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_UNSUPPORTED_ENCRYPTION);
    return 0;
  }
  // The IV parameter must be at least 8 bytes long to be used as the salt in
  // the KDF. (This should not happen given |cipher_by_name|.)
  if (EVP_CIPHER_iv_length(enc) < 8) {
    assert(0);
    OPENSSL_PUT_ERROR(PEM, PEM_R_UNSUPPORTED_ENCRYPTION);
    return 0;
  }
  if (!load_iv(header_pp, &(cipher->iv[0]), EVP_CIPHER_iv_length(enc))) {
    return 0;
  }

  return 1;
}

int PEM_def_callback(char *buf, int size, int rwflag, void *userdata) {
  if (!buf || !userdata || size < 0) {
    return 0;
  }
  size_t len = strlen((char *)userdata);
  if (len >= (size_t)size) {
    return 0;
  }
  OPENSSL_strlcpy(buf, userdata, (size_t)size);
  return len;
}

int PEM_do_header(EVP_CIPHER_INFO *cipher, unsigned char *data, long *plen,
                  pem_password_cb *callback, void *u) {
  int i = 0, j, o, klen;
  long len;
  EVP_CIPHER_CTX ctx;
  unsigned char key[EVP_MAX_KEY_LENGTH];
  char buf[PEM_BUFSIZE];

  len = *plen;

  if (cipher->cipher == NULL) {
    return 1;
  }

  klen = 0;
  if (!callback) {
    callback = PEM_def_callback;
  }
  klen = callback(buf, PEM_BUFSIZE, 0, u);
  if (klen <= 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_PASSWORD_READ);
    return 0;
  }

  if (!EVP_BytesToKey(cipher->cipher, EVP_md5(), &(cipher->iv[0]),
                      (unsigned char *)buf, klen, 1, key, NULL)) {
    return 0;
  }

  j = (int)len;
  EVP_CIPHER_CTX_init(&ctx);
  o = EVP_DecryptInit_ex(&ctx, cipher->cipher, NULL, key, &(cipher->iv[0]));
  if (o) {
    o = EVP_DecryptUpdate(&ctx, data, &i, data, j);
  }
  if (o) {
    o = EVP_DecryptFinal_ex(&ctx, &(data[i]), &j);
  }
  EVP_CIPHER_CTX_cleanup(&ctx);
  OPENSSL_cleanse((char *)buf, sizeof(buf));
  OPENSSL_cleanse((char *)key, sizeof(key));
  if (!o) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_DECRYPT);
    return 0;
  }
  j += i;
  *plen = j;
  return 1;
}

int PEM_bytes_read_bio(unsigned char **pdata, long *plen, char **pnm,
                       const char *name, BIO *bp, pem_password_cb *cb,
                       void *u) {
  EVP_CIPHER_INFO cipher;
  char *nm = NULL, *header = NULL;
  unsigned char *data = NULL;
  long len;
  int ret = 0;

  for (;;) {
    if (!PEM_read_bio(bp, &nm, &header, &data, &len)) {
      uint32_t error = ERR_peek_error();
      if (ERR_GET_LIB(error) == ERR_LIB_PEM &&
          ERR_GET_REASON(error) == PEM_R_NO_START_LINE) {
        ERR_add_error_data(2, "Expecting: ", name);
      }
      return 0;
    }
    if (check_pem(nm, name)) {
      break;
    }
    OPENSSL_free(nm);
    OPENSSL_free(header);
    OPENSSL_free(data);
  }
  if (!PEM_get_EVP_CIPHER_INFO(header, &cipher)) {
    goto err;
  }
  if (!PEM_do_header(&cipher, data, &len, cb, u)) {
    goto err;
  }

  *pdata = data;
  *plen = len;

  if (pnm) {
    *pnm = nm;
  }

  ret = 1;

err:
  if (!ret || !pnm) {
    OPENSSL_free(nm);
  }
  OPENSSL_free(header);
  if (!ret) {
    OPENSSL_free(data);
  }
  return ret;
}

void *PEM_ASN1_read_bio(d2i_of_void *d2i, const char *name, BIO *bp, void **x,
                        pem_password_cb *cb, void *u) {
  const unsigned char *p = NULL;
  unsigned char *data = NULL;
  long len;
  char *ret = NULL;

  if (!PEM_bytes_read_bio(&data, &len, NULL, name, bp, cb, u)) {
    return NULL;
  }
  p = data;
  ret = d2i(x, &p, len);
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_ASN1_LIB);
  }
  OPENSSL_free(data);
  return ret;
}
