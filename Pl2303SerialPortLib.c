/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Basic PL2303 Serial Port Library (output only).
 *
 * Can be used with EDK2's BaseDebugLibSerialPort.
 *
 * Based on the Linux pl2303 driver:
 *   Copyright (C) 2001-2007 Greg Kroah-Hartman (greg@kroah.com).
 *   Copyright (C) 2003 IBM Corp.
 * Based on BaseSerialPortLibNull:
 *   Copyright (c) 2006-2018, Intel Corporation.
 */

#include "Pl2303Dxe.h"

STATIC PL2303_PRIVATE_DATA      *mDev = NULL;
STATIC EFI_STATUS                mPermanentStatus = EFI_SUCCESS;

STATIC
VOID
Pl2303LocateSerialIo (VOID)
{
  UINTN                          HandleCount;
  UINTN                          Index;
  UINTN                          Quirks;
  INT32                          Type;
  EFI_HANDLE                    *HandleBuffer = NULL;
  EFI_STATUS                     Status;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  PL2303_PRIVATE_DATA           *Dev = NULL;

  if (mDev != NULL || EFI_ERROR (mPermanentStatus)) {
    return;
  }

  /* Boot services may not be available */
  if (gBS == NULL) {
    return;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiUsbIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    if (gBS->HandleProtocol (HandleBuffer[Index], &gEfiUsbIoProtocolGuid, (VOID **)&UsbIo) == EFI_SUCCESS &&
        Pl2303IsDeviceSupported (UsbIo, &Quirks)) {
      break;
    }
  }

  if (Index >= HandleCount) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  /* Detect chip type */
  Type = Pl2303DetectType (UsbIo);
  if (Type < 0) {
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  /* Allocate zeroed private context */
  Dev = AllocateZeroPool (sizeof (PL2303_PRIVATE_DATA));
  if (Dev == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Dev->Signature    = PL2303_PRIVATE_DATA_SIGNATURE;
  Dev->UsbIo        = UsbIo;
  Dev->Type         = (PL2303_TYPE)Type;
  Dev->Quirks       = Quirks | Pl2303TypeData[Type].Quirks;

  if ((Dev->Type == TYPE_HXD) && Pl2303IsHxdClone (UsbIo)) {
    Dev->Quirks |= PL2303_QUIRK_NO_BREAK_GETLINE;
  }

  /* Discover bulk-in / bulk-out / interrupt-in endpoints */
  Status = Pl2303DiscoverEndpoints (Dev);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  /* Set default mode values */
  Dev->Mode.ControlMask      = 0;
  Dev->Mode.Timeout          = PL2303_TIMEOUT;
  Dev->Mode.BaudRate         = PcdGet64 (PcdUartDefaultBaudRate);
  Dev->Mode.ReceiveFifoDepth = PL2303_DEFAULT_FIFO_DEPTH;
  Dev->Mode.DataBits         = PcdGet8 (PcdUartDefaultDataBits);
  Dev->Mode.Parity           = PcdGet8 (PcdUartDefaultParity);
  Dev->Mode.StopBits         = PcdGet8 (PcdUartDefaultStopBits);

  /* Run power-on initialisation */
  Status = Pl2303Startup (Dev);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  /* Reset upstream data pipes */
  if (Dev->Type == TYPE_HXN) {
    Pl2303VendorWrite (UsbIo, TRUE, PL2303_HXN_RESET_REG,
                       PL2303_HXN_RESET_UPSTREAM_PIPE |
                       PL2303_HXN_RESET_DOWNSTREAM_PIPE);
  } else if ((Dev->Quirks & PL2303_QUIRK_LEGACY) == 0) {
    Pl2303VendorWrite (UsbIo, FALSE, 8, 0);
    Pl2303VendorWrite (UsbIo, FALSE, 9, 0);
  }

  /* Program 115200 8N1 as the initial line coding */
  Status = Pl2303SetAttributes (
             Dev,
             Dev->Mode.BaudRate,
             Dev->Mode.ReceiveFifoDepth,
             Dev->Mode.Timeout,
             Dev->Mode.Parity,
             (UINT8)Dev->Mode.DataBits,
             Dev->Mode.StopBits
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:
  if (EFI_ERROR(Status)) {
    FreePool (Dev);
    mPermanentStatus = Status;
  } else {
    mDev = Dev;
  }
}

/**
  Initialize the serial device hardware.

  If no initialization is required, then return RETURN_SUCCESS.
  If the serial device was successfully initialized, then return RETURN_SUCCESS.
  If the serial device could not be initialized, then return RETURN_DEVICE_ERROR.

  @retval RETURN_SUCCESS        The serial device was initialized.
  @retval RETURN_DEVICE_ERROR   The serial device could not be initialized.

**/
RETURN_STATUS
EFIAPI
SerialPortInitialize (
  VOID
  )
{
  /* NB: We may not have gBS yet in which case the following call will do nothing */
  Pl2303LocateSerialIo ();
  return RETURN_SUCCESS;
}

/**
  Write data from buffer to serial device.

  Writes NumberOfBytes data bytes from Buffer to the serial device.
  The number of bytes actually written to the serial device is returned.
  If the return value is less than NumberOfBytes, then the write operation failed.
  If Buffer is NULL, then ASSERT().
  If NumberOfBytes is zero, then return 0.

  @param  Buffer           The pointer to the data buffer to be written.
  @param  NumberOfBytes    The number of bytes to written to the serial device.

  @retval 0                NumberOfBytes is 0.
  @retval >0               The number of bytes written to the serial device.
                           If this value is less than NumberOfBytes, then the write operation failed.

**/
UINTN
EFIAPI
SerialPortWrite (
  IN UINT8  *Buffer,
  IN UINTN  NumberOfBytes
  )
{
  EFI_STATUS  Status;

  Pl2303LocateSerialIo ();
  if (mDev == NULL) {
    return 0;
  }

  Status = Pl2303Write (mDev, &NumberOfBytes, Buffer);
  return NumberOfBytes;
}

/**
  Read data from serial device and save the data in buffer.

  Reads NumberOfBytes data bytes from a serial device into the buffer
  specified by Buffer. The number of bytes actually read is returned.
  If the return value is less than NumberOfBytes, then the rest operation failed.
  If Buffer is NULL, then ASSERT().
  If NumberOfBytes is zero, then return 0.

  @param  Buffer           The pointer to the data buffer to store the data read from the serial device.
  @param  NumberOfBytes    The number of bytes which will be read.

  @retval 0                Read data failed; No data is to be read.
  @retval >0               The actual number of bytes read from serial device.

**/
UINTN
EFIAPI
SerialPortRead (
  OUT UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  )
{
  return 0;
}

/**
  Polls a serial device to see if there is any data waiting to be read.

  Polls a serial device to see if there is any data waiting to be read.
  If there is data waiting to be read from the serial device, then TRUE is returned.
  If there is no data waiting to be read from the serial device, then FALSE is returned.

  @retval TRUE             Data is waiting to be read from the serial device.
  @retval FALSE            There is no data waiting to be read from the serial device.

**/
BOOLEAN
EFIAPI
SerialPortPoll (
  VOID
  )
{
  return FALSE;
}

/**
  Sets the control bits on a serial device.

  @param Control                Sets the bits of Control that are settable.

  @retval RETURN_SUCCESS        The new control bits were set on the serial device.
  @retval RETURN_UNSUPPORTED    The serial device does not support this operation.
  @retval RETURN_DEVICE_ERROR   The serial device is not functioning correctly.

**/
RETURN_STATUS
EFIAPI
SerialPortSetControl (
  IN UINT32  Control
  )
{
  return RETURN_UNSUPPORTED;
}

/**
  Retrieve the status of the control bits on a serial device.

  @param Control                A pointer to return the current control signals from the serial device.

  @retval RETURN_SUCCESS        The control bits were read from the serial device.
  @retval RETURN_UNSUPPORTED    The serial device does not support this operation.
  @retval RETURN_DEVICE_ERROR   The serial device is not functioning correctly.

**/
RETURN_STATUS
EFIAPI
SerialPortGetControl (
  OUT UINT32  *Control
  )
{
  return RETURN_UNSUPPORTED;
}

/**
  Sets the baud rate, receive FIFO depth, transmit/receive time out, parity,
  data bits, and stop bits on a serial device.

  @param BaudRate           The requested baud rate. A BaudRate value of 0 will use the
                            device's default interface speed.
                            On output, the value actually set.
  @param ReceiveFifoDepth   The requested depth of the FIFO on the receive side of the
                            serial interface. A ReceiveFifoDepth value of 0 will use
                            the device's default FIFO depth.
                            On output, the value actually set.
  @param Timeout            The requested time out for a single character in microseconds.
                            This timeout applies to both the transmit and receive side of the
                            interface. A Timeout value of 0 will use the device's default time
                            out value.
                            On output, the value actually set.
  @param Parity             The type of parity to use on this serial device. A Parity value of
                            DefaultParity will use the device's default parity value.
                            On output, the value actually set.
  @param DataBits           The number of data bits to use on the serial device. A DataBits
                            value of 0 will use the device's default data bit setting.
                            On output, the value actually set.
  @param StopBits           The number of stop bits to use on this serial device. A StopBits
                            value of DefaultStopBits will use the device's default number of
                            stop bits.
                            On output, the value actually set.

  @retval RETURN_SUCCESS            The new attributes were set on the serial device.
  @retval RETURN_UNSUPPORTED        The serial device does not support this operation.
  @retval RETURN_INVALID_PARAMETER  One or more of the attributes has an unsupported value.
  @retval RETURN_DEVICE_ERROR       The serial device is not functioning correctly.

**/
RETURN_STATUS
EFIAPI
SerialPortSetAttributes (
  IN OUT UINT64              *BaudRate,
  IN OUT UINT32              *ReceiveFifoDepth,
  IN OUT UINT32              *Timeout,
  IN OUT EFI_PARITY_TYPE     *Parity,
  IN OUT UINT8               *DataBits,
  IN OUT EFI_STOP_BITS_TYPE  *StopBits
  )
{
  EFI_STATUS Status;

  Pl2303LocateSerialIo ();
  if (mDev == NULL) {
    return RETURN_UNSUPPORTED;
  }

  /* NB: This call doesn't update the values */
  Status = Pl2303SetAttributes (
             mDev,
             *BaudRate,
             *ReceiveFifoDepth,
             *Timeout,
             *Parity,
             *DataBits,
             *StopBits
           );

  return EFI_ERROR (Status) ? RETURN_DEVICE_ERROR : RETURN_SUCCESS;
}
