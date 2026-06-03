/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * UEFI DXE driver for Prolific PL2303 USB to serial adapter – private header.
 *
 * Based on the Linux pl2303 driver:
 *   Copyright (C) 2001-2007 Greg Kroah-Hartman (greg@kroah.com)
 *   Copyright (C) 2003 IBM Corp.
 */

#ifndef PL2303_DXE_H_
#define PL2303_DXE_H_

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Protocol/UsbIo.h>
#include <Protocol/SerialIo.h>
#include <IndustryStandard/Usb.h>

/* Vendor / product IDs – re-use the definitions from the Linux header. */
#include "pl2303.h"

/* -----------------------------------------------------------------------
 * USB class-specific request types and requests (CDC / vendor)
 * ----------------------------------------------------------------------- */
#define SET_LINE_REQUEST_TYPE       0x21
#define SET_LINE_REQUEST            0x20

#define SET_CONTROL_REQUEST_TYPE    0x21
#define SET_CONTROL_REQUEST         0x22
#define CONTROL_DTR                 0x01
#define CONTROL_RTS                 0x02

#define BREAK_REQUEST_TYPE          0x21
#define BREAK_REQUEST               0x23
#define BREAK_ON                    0xffff
#define BREAK_OFF                   0x0000

#define GET_LINE_REQUEST_TYPE       0xa1
#define GET_LINE_REQUEST            0x21

#define VENDOR_WRITE_REQUEST_TYPE   0x40
#define VENDOR_WRITE_REQUEST        0x01
#define VENDOR_WRITE_NREQUEST       0x80

#define VENDOR_READ_REQUEST_TYPE    0xc0
#define VENDOR_READ_REQUEST         0x01
#define VENDOR_READ_NREQUEST        0x81

/* -----------------------------------------------------------------------
 * Per-device quirk flags
 * ----------------------------------------------------------------------- */
#define PL2303_QUIRK_UART_STATE_IDX0     BIT0
#define PL2303_QUIRK_LEGACY              BIT1
#define PL2303_QUIRK_ENDPOINT_HACK       BIT2
#define PL2303_QUIRK_NO_BREAK_GETLINE    BIT3

/* -----------------------------------------------------------------------
 * UART modem-status register bits (from interrupt IN notification)
 * ----------------------------------------------------------------------- */
#define UART_STATE_INDEX            8
#define UART_STATE_MSR_MASK         0x8b
#define UART_DCD                    0x01
#define UART_DSR                    0x02
#define UART_BREAK_ERROR            0x04
#define UART_RING                   0x08
#define UART_FRAME_ERROR            0x10
#define UART_PARITY_ERROR           0x20
#define UART_OVERRUN_ERROR          0x40
#define UART_CTS                    0x80

/* -----------------------------------------------------------------------
 * Flow-control register constants
 * ----------------------------------------------------------------------- */
#define PL2303_FLOWCTRL_MASK             0xf0

#define PL2303_READ_TYPE_HX_STATUS       0x8080

#define PL2303_HXN_RESET_REG             0x07
#define PL2303_HXN_RESET_UPSTREAM_PIPE   0x02
#define PL2303_HXN_RESET_DOWNSTREAM_PIPE 0x01

#define PL2303_HXN_FLOWCTRL_REG          0x0a
#define PL2303_HXN_FLOWCTRL_MASK         0x1c
#define PL2303_HXN_FLOWCTRL_NONE         0x1c
#define PL2303_HXN_FLOWCTRL_RTS_CTS      0x18
#define PL2303_HXN_FLOWCTRL_XON_XOFF    0x0c

/* -----------------------------------------------------------------------
 * Chip-type enumeration
 * ----------------------------------------------------------------------- */
typedef enum {
  TYPE_H   = 0,
  TYPE_HX,
  TYPE_TA,
  TYPE_TB,
  TYPE_HXD,
  TYPE_HXN,
  TYPE_COUNT
} PL2303_TYPE;

/* Per-type capability flags */
typedef struct {
  CONST CHAR8  *TypeName;
  UINT32        MaxBaudRate;
  UINTN         Quirks;
  BOOLEAN       NoAutoXonXoff;
  BOOLEAN       NoDivisors;
  BOOLEAN       AltDivisors;
} PL2303_TYPE_DATA;

/* -----------------------------------------------------------------------
 * Entry in the supported-device table
 * ----------------------------------------------------------------------- */
typedef struct {
  UINT16  VendorId;
  UINT16  ProductId;
  UINTN   Quirks;
} PL2303_USB_DEVICE;

/* -----------------------------------------------------------------------
 * Private driver data
 * ----------------------------------------------------------------------- */

/* Size of the internal receive ring buffer */
#define PL2303_MAX_RX_BUFFER   4096
/* Maximum bytes per single USB bulk transfer */
#define PL2303_MAX_XFER_SIZE    256

#define PL2303_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('P', 'L', '2', '3')

typedef struct {
  UINT32                    Signature;

  EFI_USB_IO_PROTOCOL      *UsbIo;

  EFI_SERIAL_IO_PROTOCOL    SerialIo;
  EFI_SERIAL_IO_MODE        Mode;

  PL2303_TYPE               Type;
  UINTN                     Quirks;

  /* Modem-control output lines (DTR / RTS) */
  UINT8                     LineControl;
  /* Last received modem-status byte from interrupt IN */
  UINT8                     LineStatus;
  /* Current 7-byte CDC line-coding (baud, stop, parity, data) */
  UINT8                     LineSettings[7];

  /* Whether hardware flow control is currently enabled */
  BOOLEAN                   HwFlowControl;

  /* Endpoint addresses (0 = not present) */
  UINT8                     BulkInAddr;
  UINT8                     BulkOutAddr;
  UINT8                     IntInAddr;

  /* Receive ring buffer */
  UINT8                     RxBuf[PL2303_MAX_RX_BUFFER];
  UINTN                     RxHead;   /* next byte to consume */
  UINTN                     RxTail;   /* next slot to fill    */
} PL2303_PRIVATE_DATA;

#define PL2303_FROM_SERIAL_IO(a) \
  CR ((a), PL2303_PRIVATE_DATA, SerialIo, PL2303_PRIVATE_DATA_SIGNATURE)

/* -----------------------------------------------------------------------
 * EFI_SERIAL_IO_PROTOCOL function prototypes
 * ----------------------------------------------------------------------- */

EFI_STATUS
EFIAPI
Pl2303SerialReset (
  IN EFI_SERIAL_IO_PROTOCOL  *This
  );

EFI_STATUS
EFIAPI
Pl2303SerialSetAttributes (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT64                   BaudRate,
  IN UINT32                   ReceiveFifoDepth,
  IN UINT32                   Timeout,
  IN EFI_PARITY_TYPE          Parity,
  IN UINT8                    DataBits,
  IN EFI_STOP_BITS_TYPE       StopBits
  );

EFI_STATUS
EFIAPI
Pl2303SerialSetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT32                   Control
  );

EFI_STATUS
EFIAPI
Pl2303SerialGetControl (
  IN  EFI_SERIAL_IO_PROTOCOL  *This,
  OUT UINT32                  *Control
  );

EFI_STATUS
EFIAPI
Pl2303SerialWrite (
  IN     EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN                   *BufferSize,
  IN     VOID                    *Buffer
  );

EFI_STATUS
EFIAPI
Pl2303SerialRead (
  IN     EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN                   *BufferSize,
  OUT    VOID                    *Buffer
  );

/* -----------------------------------------------------------------------
 * EFI_DRIVER_BINDING_PROTOCOL function prototypes
 * ----------------------------------------------------------------------- */

EFI_STATUS
EFIAPI
Pl2303DriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                    ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
Pl2303DriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                    ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
Pl2303DriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                    ControllerHandle,
  IN UINTN                         NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  );

/* -----------------------------------------------------------------------
 * Driver entry point
 * ----------------------------------------------------------------------- */

EFI_STATUS
EFIAPI
Pl2303DxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

#endif /* PL2303_DXE_H_ */
