#ifndef _USBHUB_H_
#define _USBHUB_H_

#include <ntddk.h>
#include <windef.h>
#include <stdio.h>
#include <wdm.h>
#include <wmistr.h>
#include <wmilib.h>
#include <wdmguid.h>
#include <hubbusif.h>
#include <usbbusif.h>
#include <usbdlib.h>

#define USB_HUB_TAG 'BUHU'

#define USBH_EXTENSION_TYPE_HUB        0x01
#define USBH_EXTENSION_TYPE_PORT       0x02
#define USBH_EXTENSION_TYPE_PARENT     0x04
#define USBH_EXTENSION_TYPE_FUNCTION   0x08

#define USBHUB_FDO_FLAG_DEVICE_STARTED    (1 << 0)    // 0x00000001
#define USBHUB_FDO_FLAG_DEVICE_STOPPING   (1 << 2)    // 0x00000004
#define USBHUB_FDO_FLAG_DEVICE_FAILED     (1 << 3)    // 0x00000008
#define USBHUB_FDO_FLAG_REMOTE_WAKEUP     (1 << 4)    // 0x00000010
#define USBHUB_FDO_FLAG_USB20_HUB         (1 << 15)   // 0x00008000

typedef struct _COMMON_DEVICE_EXTENSION {
  ULONG ExtensionType;
  PDEVICE_OBJECT SelfDevice;
} COMMON_DEVICE_EXTENSION, *PCOMMON_DEVICE_EXTENSION;

typedef struct _USBHUB_FDO_EXTENSION {
  COMMON_DEVICE_EXTENSION Common;
  PDEVICE_OBJECT LowerPDO;
  PDEVICE_OBJECT LowerDevice;
  PDEVICE_OBJECT RootHubPdo;
  PDEVICE_OBJECT RootHubPdo2;
  KEVENT LowerDeviceEvent;
  ULONG HubFlags;
  USB_BUS_INTERFACE_HUB_V5 BusInterface;
  USB_BUS_INTERFACE_USBDI_V2 BusInterfaceUSBDI;
  DEVICE_POWER_STATE DeviceState[POWER_SYSTEM_MAXIMUM];
  SYSTEM_POWER_STATE SystemWake;
  DEVICE_POWER_STATE DeviceWake;
  POWER_STATE CurrentPowerState;
  USB_DEVICE_DESCRIPTOR HubDeviceDescriptor;
  PUSB_CONFIGURATION_DESCRIPTOR HubConfigDescriptor;
  PUSB_HUB_DESCRIPTOR HubDescriptor;
} USBHUB_FDO_EXTENSION, *PUSBHUB_FDO_EXTENSION;

typedef struct _USBHUB_PORT_PDO_EXTENSION {
  COMMON_DEVICE_EXTENSION Common;
} USBHUB_PORT_PDO_EXTENSION, *PUSBHUB_PORT_PDO_EXTENSION;

typedef struct _USBHUB_URB_TIMEOUT_CONTEXT {
  PIRP Irp;
  KEVENT UrbTimeoutEvent;
  KDPC UrbTimeoutDPC;
  KTIMER UrbTimeoutTimer;
  KSPIN_LOCK UrbTimeoutSpinLock;
  BOOL IsNormalCompleted;
} USBHUB_URB_TIMEOUT_CONTEXT, *PUSBHUB_URB_TIMEOUT_CONTEXT;

/* ioctl.c */

NTSTATUS
NTAPI
USBH_DeviceControl(
  IN PUSBHUB_FDO_EXTENSION HubExtension,
  IN PIRP Irp);

/* pnp.c */

NTSTATUS
NTAPI
USBH_FdoPnP(
  IN PUSBHUB_FDO_EXTENSION HubExtension,
  IN PIRP Irp,
  IN UCHAR Minor);

/* power.c */

NTSTATUS
NTAPI
USBH_FdoPower(
  IN PUSBHUB_FDO_EXTENSION HubExtension,
  IN PIRP Irp,
  IN UCHAR Minor);

/* usbhub.c */

VOID
NTAPI
USBH_CompleteIrp(
  IN PIRP Irp,
  IN NTSTATUS CompleteStatus);

NTSTATUS
NTAPI
USBH_PassIrp(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp);

NTSTATUS
NTAPI
USBH_WriteFailReasonID(
  IN PDEVICE_OBJECT DeviceObject,
  IN ULONG Data);

NTSTATUS
NTAPI
USBH_FdoSyncSubmitUrb(
  IN PDEVICE_OBJECT FdoDevice,
  IN PURB Urb);

NTSTATUS
NTAPI
USBH_GetDeviceType(
  IN PUSBHUB_FDO_EXTENSION HubExtension,
  IN PUSB_DEVICE_HANDLE DeviceHandle,
  OUT USB_DEVICE_TYPE * OutDeviceType);

NTSTATUS
NTAPI
USBH_SyncGetRootHubPdo(
  IN PDEVICE_OBJECT DeviceObject,
  IN OUT PDEVICE_OBJECT * OutPdo1,
  IN OUT PDEVICE_OBJECT * OutPdo2);

NTSTATUS
NTAPI
USBH_SyncGetHubCount(
  IN PDEVICE_OBJECT DeviceObject,
  IN OUT PULONG OutHubCount);

PUSB_DEVICE_HANDLE
NTAPI
USBH_SyncGetDeviceHandle(
  IN PDEVICE_OBJECT DeviceObject);

NTSTATUS
NTAPI
USBH_GetDeviceDescriptor(
  IN PDEVICE_OBJECT DeviceObject,
  IN PUSB_DEVICE_DESCRIPTOR HubDeviceDescriptor);

NTSTATUS
NTAPI
USBH_GetConfigurationDescriptor(
  IN PDEVICE_OBJECT DeviceObject,
  IN PUSB_CONFIGURATION_DESCRIPTOR * pConfigurationDescriptor);

NTSTATUS
NTAPI
DriverEntry(
  IN PDRIVER_OBJECT DriverObject,
  IN PUNICODE_STRING RegistryPath);

#endif /* _USBHUB_H_ */
