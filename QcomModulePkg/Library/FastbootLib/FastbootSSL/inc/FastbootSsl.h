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
 * [including the GNU Public Licence.]
 */

/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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

#ifndef __FASTBOOT_SSL_H__
#define __FASTBOOT_SSL_H__

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include "../inc/FastbootSslBase.h"
#include <OpenDice/boringssl/src/include/openssl/opensslconf.h>
#include <openssl/base.h>
#include <openssl/nid.h>

/* ====================================================================
                            // Generic //
   ====================================================================*/
#define FASTBOOT_SSL_COMPAT

#define INT32_MAX   0x7fffffff
#define INT_MAX INT32_MAX

//#undef UINT32_MAX
#define UINT32_MAX  0xffffffff
#define UINT_MAX  UINT32_MAX
#define UINT64_MAX (UINT64_C(0xFFFFFFFFFFFFFFFF))

#ifdef MDE_CPU_AARCH64
# define UINT64_C(x)                       x##UL
#else
# ifdef MDE_CPU_ARM
#  define UINT64_C(x)                       x##ULL
# else
#  error Need UINT64_C definition for this architecture
# endif
#endif

#define SIZE_MAX MAX_UINTN

#define MAX_STRING_SIZE  0x1000

#define OPENSSL_MSVC_PRAGMA(arg)
#define OPENSSL_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#define va_arg VA_ARG
#define va_list VA_LIST
#define va_start VA_START
#define va_end VA_END
#define va_copy VA_COPY

#define OPENSSL_malloc                    OPENSSL_malloc2
#define OPENSSL_free                      OPENSSL_free2
#define OPENSSL_cleanse                   OPENSSL_cleanse2

#define malloc(n)                         AllocatePool(n)
#define free(ptr)                         if (ptr != NULL) { FreePool(ptr); }
#define static_assert(expr, err)
#define assert(expression)                ASSERT(expression)
#define abort()                           assert(0)
#define memcmp(buf1,buf2,count)           (int)(CompareMem(buf1,buf2,(UINTN)(count)))
#define memcpy(dest,source,count)         CopyMem(dest,source,(UINTN)(count))
#define memset(dest,ch,count)             SetMem(dest,(UINTN)(count),(UINT8)(ch))
#define memmove(dest,source,count)        CopyMem(dest,source,(UINTN)(count))
#define memchr(buf,ch,count)              ScanMem8(buf,(UINTN)(count),(UINT8)ch)
#define strlen(str)                       (size_t)(AsciiStrnLenS(str,MAX_STRING_SIZE))
#define strcmp(string1,string2)           (int)(AsciiStriCmp(string1,string2))
#define strncmp(string1,string2,count)    (int)(AsciiStrnCmp(string1,string2,(UINTN)(count)))
#define vsnprintf                         (int)AsciiVSPrint
#define alignas                           _Alignas

/* ====================================================================
                             // Err //
   ====================================================================*/

#define ENOMEM 12

/* ====================================================================
                             // Bio //
   ====================================================================*/

extern BIO *fastboot_bio;

/* ====================================================================
                             // RSA //
   ====================================================================*/

extern RSA *fastboot_publicRsa;

/* ====================================================================
                    // Functions Declarations //
   ====================================================================*/

#endif
