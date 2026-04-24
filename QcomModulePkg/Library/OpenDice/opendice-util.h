/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __OPENDICE_UTIL_H__
#define __OPENDICE_UTIL_H__

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/Debug.h>
#include <Library/MemoryAllocationLib.h>

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
#define SIZE_MAX MAX_UINTN

#define va_list VA_LIST
#define va_start VA_START
#define va_end VA_END
#define va_copy VA_COPY

typedef int ptrdiff_t;

// FILE is not used, but define it to make headers happy
typedef void FILE;

#define memcpy(dest,source,count)         CopyMem(dest,source,(UINTN)(count))
#define memset(dest,ch,count)             SetMem(dest,(UINTN)(count),(UINT8)(ch))
#define memchr(buf,ch,count)              ScanMem8(buf,(UINTN)(count),(UINT8)ch)
#define memcmp(buf1,buf2,count)           (int)(CompareMem(buf1,buf2,(UINTN)(count)))
#define memmove(dest,source,count)        CopyMem(dest,source,(UINTN)(count))
#define strlen(str)                       (size_t)(AsciiStrnLenS(str,MAX_STRING_SIZE))
#define strcpy(strDest,strSource)         AsciiStrCpyS(strDest,MAX_STRING_SIZE,strSource)
#define strncpy(strDest,strSource,count)  AsciiStrnCpyS(strDest,MAX_STRING_SIZE,strSource,(UINTN)count)
#define strcat(strDest,strSource)         AsciiStrCatS(strDest,MAX_STRING_SIZE,strSource)
#define strncmp(string1,string2,count)    (int)(AsciiStrnCmp(string1,string2,(UINTN)(count)))
#define strcasecmp(str1,str2)             (int)AsciiStriCmp(str1,str2)
#define sprintf(buf,...)                  AsciiSPrint(buf,MAX_STRING_SIZE,__VA_ARGS__)
#define localtime(timer)                  NULL
#define assert(expression)                ASSERT(expression)
#define static_assert(expr, err)
#define offsetof(type,member)             OFFSET_OF(type,member)
#define atoi(nptr)                        AsciiStrDecimalToUintn(nptr)
#define gettimeofday(tvp,tz)              do { (tvp)->tv_sec = time(NULL); (tvp)->tv_usec = 0; } while (0)
#define vsnprintf                         (int)AsciiVSPrint

# define UINT8_C(x)                        x
# define UINT16_C(x)                       x
# define UINT32_C(x)                       x##U

#ifdef MDE_CPU_AARCH64
# define UINT64_C(x)                       x##UL
#else
# ifdef MDE_CPU_ARM
#  define UINT64_C(x)                       x##ULL
# else
#  error Need UINT64_C definition for this architecture
# endif
#endif

#define INT64_MAX ((INT64)UINT64_C(0x7FFFFFFFFFFFFFFF))
#define UINT64_MAX (UINT64_C(0xFFFFFFFFFFFFFFFF))

#define MAX_STRING_SIZE  0x1000
VOID *OPENSSL_malloc (size_t Size);
VOID OPENSSL_free (VOID *OrigPtr);

#endif
