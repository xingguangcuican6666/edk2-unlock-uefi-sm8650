/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/* This file should be used by AVB only.
   It does not replace a full SMCInvoke implementation in EDK2. */

#ifndef SMCI_UTILS_H
#define SMCI_UTILS_H

#include <Uefi.h>

typedef UINTN SizeT;
typedef UINT32 ObjectOp;

#define ObjectOp_METHOD_MASK ((ObjectOp)0x0000FFFFu)
//------------------------------------------------------------------------
// ObjectCounts
//
// The number and kinds of arguments passed to invoke are encoded in a
// 32-bit quantity `ObjectCounts`. Currently only 16-bits are used; the
// remainder are reserved for future enhancements.
//------------------------------------------------------------------------

typedef UINT32 ObjectCounts;

#define ObjectCounts_pack(nBuffersIn, nBuffersOut, nObjectsIn, nObjectsOut)    \
  ((ObjectCounts)((nBuffersIn) | ((nBuffersOut) << 4) | ((nObjectsIn) << 8) |  \
                  ((nObjectsOut) << 12)))

typedef union ObjectArg ObjectArg;
typedef VOID *ObjectCxt;

typedef INT32 (*ObjectInvoke) (ObjectCxt ArgH,
                                 ObjectOp Op,
                                 ObjectArg *Args,
                                 ObjectCounts Counts);

typedef struct Object {
  ObjectInvoke Invoke;
  ObjectCxt Context; // context data to pass to the invoke function
} Object;

typedef struct ObjectBuf {
  VOID *Ptr;
  SizeT Size;
} ObjectBuf;

typedef struct ObjectBufIn {
  CONST VOID *Ptr;
  SizeT Size;
} ObjectBufIn;

union ObjectArg {
  ObjectBuf ArgB;
  ObjectBufIn ArgBi;
  Object ArgO;
};

static inline INT32
ObjectInvokeFunc (Object ArgO, ObjectOp Op, ObjectArg *Args, ObjectCounts ArgK)
{
  return ArgO.Invoke (ArgO.Context, Op, Args, ArgK);
}

#define Object_NULL ((Object) {NULL, NULL})
#define Object_isERROR(err) ((err) != 0)

//------------------------------------------------------------------------
// Base interface: `Object` also serves as the name of the interface
//     that all interfaces implicitly inherit.
//------------------------------------------------------------------------

#define Object_OP_Release (ObjectOp_METHOD_MASK - 0)
#define Object_OP_Retain (ObjectOp_METHOD_MASK - 1)

#define Object_Release(o) ObjectInvokeFunc ((o), Object_OP_Release, 0, 0)
#define Object_Retain(o) ObjectInvokeFunc ((o), Object_OP_Retain, 0, 0)

#define Object_isNull(O) ((O).Invoke == NULL)

static inline VOID
Object_Replace (Object *Loc, Object ObjNew)
{
  if (!Object_isNull (*Loc)) {
    Object_Release (*Loc);
  }
  if (!Object_isNull (ObjNew)) {
    Object_Retain (ObjNew);
  }
  *Loc = ObjNew;
}

#define Object_ASSIGN(Loc, Obj) Object_Replace (&(Loc), (Obj))
#define Object_ASSIGN_NULL(Loc) Object_Replace (&(Loc), Object_NULL)

#define IAppClient_OP_getAppObject 0

static inline INT32
IAppClientGetAppObject (Object Self,
                        CONST VOID *AppDistNamePtr,
                        SizeT AppDistNameLen,
                        Object *ObjPtr)
{
  ObjectArg ArgA[2];
  ArgA[0].ArgBi = (ObjectBufIn) {AppDistNamePtr, AppDistNameLen * 1};

  INT32 Result = ObjectInvokeFunc (Self, IAppClient_OP_getAppObject,
                                     ArgA,
                                     ObjectCounts_pack (1, 0, 0, 1));

  *ObjPtr = ArgA[1].ArgO;

  return Result;
}

#define IClientEnv_OP_open 0

static inline INT32
IClientEnvOpen (Object Self, UINT32 uIdVal, Object *ObjPtr)
{
  ObjectArg ArgA[2];
  ArgA[0].ArgB = (ObjectBuf) {&uIdVal, sizeof (UINT32)};

  INT32 Result = ObjectInvokeFunc (Self, IClientEnv_OP_open, ArgA,
                                     ObjectCounts_pack (1, 0, 0, 1));

  *ObjPtr = ArgA[1].ArgO;

  return Result;
}

#define IOpener_OP_open 0

static inline INT32
IOpener_open (Object Self, UINT32 IdVal, Object *ObjPtr)
{
  ObjectArg ArgA[2];
  ArgA[0].ArgB = (ObjectBuf) {&IdVal, sizeof (UINT32)};

  INT32 Result = ObjectInvokeFunc (Self, IOpener_OP_open, ArgA,
                                     ObjectCounts_pack (1, 0, 0, 1));

  *ObjPtr = ArgA[1].ArgO;

  return Result;
}

#define IKMHal_OP_sendCmd 0

static inline INT32
IKMHal_sendCmd (Object Self,
                CONST VOID *InputPtr,
                SizeT InputLen,
                VOID *OutputPtr,
                SizeT OutputLen,
                SizeT *OutputLenout)
{
  ObjectArg ArgA[2] = {{{0, 0}}};
  ArgA[0].ArgBi = (ObjectBufIn) {InputPtr, InputLen * 1};
  ArgA[1].ArgB = (ObjectBuf) {OutputPtr, OutputLen * 1};

  INT32 Result = ObjectInvokeFunc (Self, IKMHal_OP_sendCmd, ArgA,
                                     ObjectCounts_pack (1, 1, 0, 0));

  *OutputLenout = ArgA[1].ArgB.Size / 1;

  return Result;
}
#endif /* SMCI_UTILS_H */

