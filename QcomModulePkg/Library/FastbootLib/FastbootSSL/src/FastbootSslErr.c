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
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
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

struct err_error_st {
  // file contains the filename where the error occurred.
  const char *file;
  // data contains a NUL-terminated string with optional data. It is allocated
  // with system |malloc| and must be freed with |free| (not |OPENSSL_free|)
  char *data;
  // packed contains the error library and reason, as packed by ERR_PACK.
  uint32_t packed;
  // line contains the line number where the error occurred.
  uint16_t line;
  // mark indicates a reversion point in the queue. See |ERR_pop_to_mark|.
  unsigned mark : 1;
};

// ERR_STATE contains the per-thread, error queue.
typedef struct err_state_st {
  // errors contains the ERR_NUM_ERRORS most recent errors, organised as a ring
  // buffer.
  struct err_error_st errors[ERR_NUM_ERRORS];
  // top contains the index one past the most recent error. If |top| equals
  // |bottom| then the queue is empty.
  unsigned top;
  // bottom contains the index of the last error in the queue.
  unsigned bottom;

  // to_free, if not NULL, contains a pointer owned by this structure that was
  // previously a |data| pointer of one of the elements of |errors|.
  void *to_free;
} ERR_STATE;

static ERR_STATE *err_get_state(void);
static void err_clear(struct err_error_st *error);

#ifdef FASTBOOT_SSL_COMPAT
static int errno;

static char *strdup(const char *s) {
  if (s == NULL) {
    return NULL;
  }
  const size_t len = strlen(s) + 1;
  char *ret = AllocateZeroPool(len);
  if (ret == NULL) {
    return NULL;
  }
  memcpy(ret, s, len);
  return ret;
}
#endif

static void err_set_error_data(char *data) {
  ERR_STATE *const state = err_get_state();
  struct err_error_st *error;

  if (state == NULL || state->top == state->bottom) {
    free(data);
    return;
  }

  error = &state->errors[state->top];

  free(error->data);
  error->data = data;
}

void ERR_add_error_dataf(const char *format, ...) {
  char *buf = NULL;
  va_list ap;

  va_start(ap, format);
  if (OPENSSL_vasprintf_internal(&buf, format, ap, /*system_malloc=*/1) == -1) {
    return;
  }
  va_end(ap);

  err_set_error_data(buf);
}

void ERR_set_error_data(char *data, int flags) {
  if (!(flags & ERR_FLAG_STRING)) {
    // We do not support non-string error data.
    assert(0);
    return;
  }
  // Disable deprecated functions on msvc so it doesn't complain about strdup.
  OPENSSL_MSVC_PRAGMA(warning(push))
  OPENSSL_MSVC_PRAGMA(warning(disable : 4996))
  // We can not use OPENSSL_strdup because we don't want to call OPENSSL_malloc,
  // which can affect the error stack.
  char *copy = strdup(data);
  OPENSSL_MSVC_PRAGMA(warning(pop))
  if (copy != NULL) {
    err_set_error_data(copy);
  }
  if (flags & ERR_FLAG_MALLOCED) {
    // We can not take ownership of |data| directly because it is allocated with
    // |OPENSSL_malloc| and we will free it with system |free| later.
    OPENSSL_free(data);
  }
}

static void err_add_error_vdata(unsigned num, va_list args) {
  size_t total_size = 0;
  const char *substr;
  char *buf;

  va_list args_copy;
  va_copy(args_copy, args);
  for (size_t i = 0; i < num; i++) {
    substr = va_arg(args_copy, const char *);
    if (substr == NULL) {
      continue;
    }
    size_t substr_len = strlen(substr);
    if (SIZE_MAX - total_size < substr_len) {
      return; // Would overflow.
    }
    total_size += substr_len;
  }
  va_end(args_copy);
  if (total_size == SIZE_MAX) {
      return; // Would overflow.
  }
  total_size += 1; // NUL terminator.
  if ((buf = malloc(total_size)) == NULL) {
    return;
  }
  buf[0] = '\0';
  for (size_t i = 0; i < num; i++) {
    substr = va_arg(args, const char *);
    if (substr == NULL) {
      continue;
    }
    if (OPENSSL_strlcat(buf, substr, total_size) >= total_size) {
      assert(0); // should not be possible.
    }
  }
  va_end(args);
  err_set_error_data(buf);
}

void ERR_add_error_data(unsigned count, ...) {
  va_list args;
  va_start(args, count);
  err_add_error_vdata(count, args);
  va_end(args);
}

static uint32_t get_error_values(int inc, int top, const char **file, int *line,
                                 const char **data, int *flags) {
  unsigned i = 0;
  ERR_STATE *state;
  struct err_error_st *error;
  uint32_t ret;

  state = err_get_state();
  if (state == NULL || state->bottom == state->top) {
    return 0;
  }

  if (top) {
    assert(!inc);
    // last error
    i = state->top;
  } else {
    i = (state->bottom + 1) % ERR_NUM_ERRORS;
  }

  error = &state->errors[i];
  ret = error->packed;

  if (file != NULL && line != NULL) {
    if (error->file == NULL) {
      *file = "NA";
      *line = 0;
    } else {
      *file = error->file;
      *line = error->line;
    }
  }

  if (data != NULL) {
    if (error->data == NULL) {
      *data = "";
      if (flags != NULL) {
        *flags = 0;
      }
    } else {
      *data = error->data;
      if (flags != NULL) {
        // Without |ERR_FLAG_MALLOCED|, rust-openssl assumes the string has a
        // static lifetime. In both cases, we retain ownership of the string,
        // and the caller is not expected to free it.
        *flags = ERR_FLAG_STRING | ERR_FLAG_MALLOCED;
      }
      // If this error is being removed, take ownership of data from
      // the error. The semantics are such that the caller doesn't
      // take ownership either. Instead the error system takes
      // ownership and retains it until the next call that affects the
      // error queue.
      if (inc) {
        if (error->data != NULL) {
          free(state->to_free);
          state->to_free = error->data;
        }
        error->data = NULL;
      }
    }
  }

  if (inc) {
    assert(!top);
    err_clear(error);
    state->bottom = i;
  }

  return ret;
}

uint32_t ERR_peek_error(void) {
  return get_error_values(0 /* peek */, 0 /* bottom */, NULL, NULL, NULL, NULL);
}

static void err_clear(struct err_error_st *error) {
  free(error->data);
  OPENSSL_memset(error, 0, sizeof(struct err_error_st));
}

void ERR_clear_error(void) {
  ERR_STATE *const state = err_get_state();
  unsigned i;

  if (state == NULL) {
    return;
  }

  for (i = 0; i < ERR_NUM_ERRORS; i++) {
    err_clear(&state->errors[i]);
  }
  free(state->to_free);
  state->to_free = NULL;

  state->top = state->bottom = 0;
}

static void err_state_free(void *statep) {
  ERR_STATE *state = statep;

  if (state == NULL) {
    return;
  }

  for (unsigned i = 0; i < ERR_NUM_ERRORS; i++) {
    err_clear(&state->errors[i]);
  }
  free(state->to_free);
  free(state);
}

// err_get_state gets the ERR_STATE object for the current thread.
static ERR_STATE *err_get_state(void) {
  ERR_STATE *state = CRYPTO_get_thread_local(OPENSSL_THREAD_LOCAL_ERR);
  if (state == NULL) {
    state = malloc(sizeof(ERR_STATE));
    if (state == NULL) {
      return NULL;
    }
    OPENSSL_memset(state, 0, sizeof(ERR_STATE));
    if (!CRYPTO_set_thread_local(OPENSSL_THREAD_LOCAL_ERR, state,
                                 err_state_free)) {
      return NULL;
    }
  }

  return state;
}

void ERR_put_error(int library, int unused, int reason, const char *file,
                   unsigned line) {
  ERR_STATE *const state = err_get_state();
  struct err_error_st *error;

  if (state == NULL) {
    return;
  }

  if (library == ERR_LIB_SYS && reason == 0) {
#if defined(OPENSSL_WINDOWS)
    reason = GetLastError();
#else
    reason = errno;
#endif
  }

  state->top = (state->top + 1) % ERR_NUM_ERRORS;
  if (state->top == state->bottom) {
    state->bottom = (state->bottom + 1) % ERR_NUM_ERRORS;
  }

  error = &state->errors[state->top];
  err_clear(error);
  error->file = file;
  error->line = line;
  error->packed = ERR_PACK(library, reason);
}
