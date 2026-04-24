/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "opendice-util.h"

int errno;

VOID *OPENSSL_malloc (size_t Size)
{
  return AllocateZeroPool (Size);
}

VOID OPENSSL_free (VOID *OrigPtr)
{
  if (!OrigPtr) {
    return;
  }

  FreePool (OrigPtr);
}

void OPENSSL_cleanse(void *ptr, size_t len)
{
  memset(ptr, 0, len);
}
