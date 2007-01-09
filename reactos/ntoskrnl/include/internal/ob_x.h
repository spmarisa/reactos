/*
* PROJECT:         ReactOS Kernel
* LICENSE:         GPL - See COPYING in the top level directory
* FILE:            ntoskrnl/include/ob_x.h
* PURPOSE:         Intenral Inlined Functions for the Object Manager
* PROGRAMMERS:     Alex Ionescu (alex.ionescu@reactos.org)
*/

#include "ex.h"

#if DBG
VOID
FORCEINLINE
ObpCalloutStart(IN PKIRQL CalloutIrql)
{
    /* Save the callout IRQL */
    *CalloutIrql = KeGetCurrentIrql();
}

VOID
FORCEINLINE
ObpCalloutEnd(IN KIRQL CalloutIrql,
              IN PCHAR Procedure,
              IN POBJECT_TYPE ObjectType,
              IN PVOID Object)
{
    /* Detect IRQL change */
    if (CalloutIrql != KeGetCurrentIrql())
    {
        /* Print error */
        DbgPrint("OB: ObjectType: %wZ  Procedure: %s  Object: %08x\n",
                 &ObjectType->Name, Procedure, Object);
        DbgPrint("    Returned at %x IRQL, but was called at %x IRQL\n",
                 KeGetCurrentIrql(), CalloutIrql);
        DbgBreakPoint();
    }
}
#else
VOID
FORCEINLINE
ObpCalloutStart(IN PKIRQL CalloutIrql)
{
    /* No-op */
    UNREFERENCED_PARAMETER(CalloutIrql);
}

VOID
FORCEINLINE
ObpCalloutEnd(IN KIRQL CalloutIrql,
              IN PCHAR Procedure,
              IN POBJECT_TYPE ObjectType,
              IN PVOID Object)
{
    UNREFERENCED_PARAMETER(CalloutIrql);
}
#endif

VOID
FORCEINLINE
_ObpAcquireDirectoryLockShared(IN POBJECT_DIRECTORY Directory,
                               IN POBP_LOOKUP_CONTEXT Context)
{
    /* It's not, set lock flag */
    Context->LockStateSignature = 0xBBBB1234;

    /* Lock it */
    KeEnterCriticalRegion();
    ExAcquirePushLockShared(&Directory->Lock);

    /* Update lock flag */
    Context->LockStateSignature = 0xDDDD1234;
}

VOID
FORCEINLINE
_ObpAcquireDirectoryLockExclusive(IN POBJECT_DIRECTORY Directory,
                                  IN POBP_LOOKUP_CONTEXT Context)
{
    /* Update lock flag */
    Context->LockStateSignature = 0xAAAA1234;

    /* Lock it */
    KeEnterCriticalRegion();
    ExAcquirePushLockExclusive(&Directory->Lock);
}

VOID
FORCEINLINE
_ObpReleaseDirectoryLock(IN POBJECT_DIRECTORY Directory,
                         IN POBP_LOOKUP_CONTEXT Context)
{
    /* Release the lock */
    ExReleasePushLock(&Directory->Lock);
    Context->LockStateSignature = 0xEEEE1234;
    KeLeaveCriticalRegion();
}

ULONG
FORCEINLINE
ObpIncrementQueryReference(IN POBJECT_HEADER ObjectHeader,
                           IN POBJECT_HEADER_NAME_INFO ObjectNameInfo)
{
    ULONG NewValue, References;

    /* Get the number of references */
    NewValue = ObjectNameInfo->QueryReferences;
    while ((NewValue != 0) && (References = NewValue))
    {
        /* Increment the number of references */
        if (InterlockedCompareExchange(&ObjectNameInfo->QueryReferences,
                                       NewValue + 1,
                                       NewValue) == References)
        {
            /* Check if the object is to be deferred deleted */
            if (ObjectHeader->Flags & OB_FLAG_DEFER_DELETE)
            {
                /* FIXME: Unhandled*/
                DbgPrint("OB: Unhandled path\n");
                KEBUGCHECK(0);
            }

            /* Done looping */
            NewValue = ObjectNameInfo->QueryReferences;
            break;
        }
    }

    /* Return the number of references */
    return NewValue;
}

VOID
FORCEINLINE
_ObpDecrementQueryReference(IN POBJECT_HEADER_NAME_INFO HeaderNameInfo)
{
    POBJECT_DIRECTORY Directory;

    /* Remove a query reference and check if it was the last one */
    if (!InterlockedExchangeAdd(&HeaderNameInfo->QueryReferences, -1))
    {
        /* Check if we have a name */
        if (HeaderNameInfo->Name.Buffer)
        {
            /* We can get rid of the object name now */
            ExFreePool(HeaderNameInfo->Name.Buffer);
            RtlInitEmptyUnicodeString(&HeaderNameInfo->Name, NULL, 0);
        }

        /* Check if the object has a directory associated to it */
        Directory = HeaderNameInfo->Directory;
        if (Directory)
        {
            /* Delete the directory */
            HeaderNameInfo->Directory = NULL;
            ObDereferenceObjectDeferDelete(Directory);
        }
    }
}

VOID
FORCEINLINE
_ObpCleanupDirectoryLookup(IN POBP_LOOKUP_CONTEXT Context,
                           IN BOOLEAN DereferenceObject)
{
    POBJECT_HEADER ObjectHeader;
    POBJECT_HEADER_NAME_INFO HeaderNameInfo;

    /* Check if we came back with the directory locked */
    if (Context->DirectoryLocked)
    {
        /* Release the lock */
        _ObpReleaseDirectoryLock(Context->Directory, Context);
    }

    /* Clear the context  */
    Context->Directory = NULL;
    Context->DirectoryLocked = FALSE;

    /* Check if we had found an object */
    if (Context->Object)
    {
        /* Get the object name information */
        ObjectHeader = OBJECT_TO_OBJECT_HEADER(Context->Object);
        HeaderNameInfo = OBJECT_HEADER_TO_NAME_INFO(ObjectHeader);

        /* Check if we do have name information */
        if (HeaderNameInfo) _ObpDecrementQueryReference(HeaderNameInfo);

        /* Check if we need to dereference it */
        if (DereferenceObject) ObDereferenceObject(Context->Object);
    }
}

VOID
FORCEINLINE
_ObpInitializeDirectoryLookup(IN POBP_LOOKUP_CONTEXT Context)
{
    /* Initialize a null context */
    Context->Object = NULL;
    Context->Directory = NULL;
    Context->DirectoryLocked = FALSE;
    Context->LockStateSignature = 0xFFFF1234;
}

#if _OB_DEBUG_
#define ObpAcquireDirectoryLockShared(a, b)                                 \
{                                                                           \
    DbgPrint("OB QUERY: Acquiring lock at %s %d\n", __FUNCTION__, __LINE__);\
    _ObpAcquireDirectoryLockShared(a, b);                                   \
}
#define ObpAcquireDirectoryLockExclusive(a, b)                              \
{                                                                           \
    DbgPrint("OB QUERY: Acquiring lock at %s %d\n", __FUNCTION__, __LINE__);\
    _ObpAcquireDirectoryLockExclusive(a, b);                                \
}
#define ObpReleaseDirectoryLock(a, b)                                       \
{                                                                           \
    DbgPrint("OB QUERY: Releasing lock at %s %d\n", __FUNCTION__, __LINE__);\
    _ObpReleaseDirectoryLock(a, b);                                         \
}
#define ObpInitializeDirectoryLookup(a)                                     \
{                                                                           \
    DbgPrint("OB QUERY: Initialization at %s %d\n", __FUNCTION__, __LINE__);\
    _ObpInitializeDirectoryLookup(a);                                       \
}
#define ObpCleanupDirectoryLookup(a, b)                                     \
{                                                                           \
    DbgPrint("OB QUERY: Cleanup at %s %d\n", __FUNCTION__, __LINE__);       \
    _ObpCleanupDirectoryLookup(a, b);                                       \
}
#define ObpDecrementQueryReference(a)                                       \
{                                                                           \
    DbgPrint("OB QUERY: Decrement at %s %d\n", __FUNCTION__, __LINE__);     \
    _ObpDecrementQueryReference(a);                                         \
}
#else
#define ObpDecrementQueryReference          _ObpDecrementQueryReference
#define ObpAcquireDirectoryLockExclusive    _ObpAcquireDirectoryLockExclusive
#define ObpAcquireDirectoryLockShared       _ObpAcquireDirectoryLockShared
#define ObpReleaseDirectoryLock             _ObpReleaseDirectoryLock
#define ObpInitializeDirectoryLookup        _ObpInitializeDirectoryLookup
#define ObpCleanupDirectoryLookup           _ObpCleanupDirectoryLookup
#endif

VOID
FORCEINLINE
ObpEnterObjectTypeMutex(IN POBJECT_TYPE ObjectType)
{
    /* Sanity check */
    ASSERT(KeGetCurrentIrql() <= APC_LEVEL);

    /* Enter a critical region and acquire the resource */
    KeEnterCriticalRegion();
    ExAcquireResourceExclusiveLite(&ObjectType->Mutex, TRUE);
}

VOID
FORCEINLINE
ObpLeaveObjectTypeMutex(IN POBJECT_TYPE ObjectType)
{
    /* Enter a critical region and acquire the resource */
    ExReleaseResourceLite(&ObjectType->Mutex);
    KeLeaveCriticalRegion();

    /* Sanity check */
    ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
}

VOID
FORCEINLINE
ObpReleaseCapturedAttributes(IN POBJECT_CREATE_INFORMATION ObjectCreateInfo)
{
    /* Check if we have a security descriptor */
    if (ObjectCreateInfo->SecurityDescriptor)
    {
        /* Release it */
        SeReleaseSecurityDescriptor(ObjectCreateInfo->SecurityDescriptor,
                                    ObjectCreateInfo->ProbeMode,
                                    TRUE);
        ObjectCreateInfo->SecurityDescriptor = NULL;
    }
}

PVOID
FORCEINLINE
ObpAllocateCapturedAttributes(IN PP_NPAGED_LOOKASIDE_NUMBER Type)
{
    PVOID Buffer;
    PNPAGED_LOOKASIDE_LIST List;
    PKPRCB Prcb = KeGetCurrentPrcb();

    /* Get the P list first */
    List = (PNPAGED_LOOKASIDE_LIST)Prcb->PPLookasideList[Type].P;

    /* Attempt allocation */
    List->L.TotalAllocates++;
    Buffer = (PVOID)InterlockedPopEntrySList(&List->L.ListHead);
    if (!Buffer)
    {
        /* Let the balancer know that the P list failed */
        List->L.AllocateMisses++;

        /* Try the L List */
        List = (PNPAGED_LOOKASIDE_LIST)Prcb->PPLookasideList[Type].L;
        List->L.TotalAllocates++;
        Buffer = (PVOID)InterlockedPopEntrySList(&List->L.ListHead);
        if (!Buffer)
        {
            /* Let the balancer know the L list failed too */
            List->L.AllocateMisses++;

            /* Allocate it */
            Buffer = List->L.Allocate(List->L.Type, List->L.Size, List->L.Tag);
        }
    }

    /* Return buffer */
    return Buffer;
}

VOID
FORCEINLINE
ObpFreeCapturedAttributes(IN PVOID Buffer,
                          IN PP_NPAGED_LOOKASIDE_NUMBER Type)
{
    PNPAGED_LOOKASIDE_LIST List;
    PKPRCB Prcb = KeGetCurrentPrcb();

    /* Use the P List */
    List = (PNPAGED_LOOKASIDE_LIST)Prcb->PPLookasideList[Type].P;
    List->L.TotalFrees++;

    /* Check if the Free was within the Depth or not */
    if (ExQueryDepthSList(&List->L.ListHead) >= List->L.Depth)
    {
        /* Let the balancer know */
        List->L.FreeMisses++;

        /* Use the L List */
        List = (PNPAGED_LOOKASIDE_LIST)Prcb->PPLookasideList[Type].L;
        List->L.TotalFrees++;

        /* Check if the Free was within the Depth or not */
        if (ExQueryDepthSList(&List->L.ListHead) >= List->L.Depth)
        {
            /* All lists failed, use the pool */
            List->L.FreeMisses++;
            List->L.Free(Buffer);
        }
    }
    else
    {
        /* The free was within the Depth */
        InterlockedPushEntrySList(&List->L.ListHead,
                                  (PSINGLE_LIST_ENTRY)Buffer);
    }
}

VOID
FORCEINLINE
ObpFreeAndReleaseCapturedAttributes(IN POBJECT_CREATE_INFORMATION ObjectCreateInfo)
{
    /* First release the attributes, then free them from the lookaside list */
    ObpReleaseCapturedAttributes(ObjectCreateInfo);
    ObpFreeCapturedAttributes(ObjectCreateInfo, LookasideCreateInfoList);
}



