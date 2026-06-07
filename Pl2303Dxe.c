// SPDX-License-Identifier: GPL-2.0+
/*
 * UEFI DXE driver for Prolific PL2303 USB to serial adapter.
 *
 * Implements EFI_SERIAL_IO_PROTOCOL on top of EFI_USB_IO_PROTOCOL for
 * the PL2303 family of USB-to-serial bridge chips.
 *
 * Based on the Linux pl2303 driver:
 *   Copyright (C) 2001-2007 Greg Kroah-Hartman (greg@kroah.com)
 *   Copyright (C) 2003 IBM Corp.
 */

#include "Pl2303Dxe.h"

/* =========================================================================
 * Global driver-binding instance
 * ========================================================================= */

EFI_DRIVER_BINDING_PROTOCOL Pl2303DriverBinding = {
  Pl2303DriverBindingSupported,
  Pl2303DriverBindingStart,
  Pl2303DriverBindingStop,
  0x10,
  NULL,
  NULL
};

/* =========================================================================
 * Component Name
 * ========================================================================= */

STATIC CHAR16* Pl2303DriverName = L"PL2303 USB Serial Driver";

EFI_STATUS
EFIAPI
Pl2303GetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                      **DriverName
  )
{
  *DriverName = Pl2303DriverName;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
Pl2303GetDriverName2 (
  IN  EFI_COMPONENT_NAME2_PROTOCOL  *This,
  IN  CHAR8                         *Language,
  OUT CHAR16                       **DriverName
  )
{
  *DriverName = Pl2303DriverName;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
Pl2303GetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  EFI_HANDLE                    ControllerHandle,
  IN  EFI_HANDLE                    ChildHandle      OPTIONAL,
  IN  CHAR8                        *Language,
  OUT CHAR16                      **ControllerName
  )
{
  EFI_STATUS              Status;
  EFI_SERIAL_IO_PROTOCOL  *SerialIo;
  EFI_USB_IO_PROTOCOL     *UsbIoProtocol;

  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  /* Check Controller's handle */
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **) &UsbIoProtocol,
                  Pl2303DriverBinding.DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (!EFI_ERROR (Status)) {
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiUsbIoProtocolGuid,
           Pl2303DriverBinding.DriverBindingHandle,
           ControllerHandle
           );

    return EFI_UNSUPPORTED;
  }

  if (Status != EFI_ALREADY_STARTED) {
    return EFI_UNSUPPORTED;
  }

  /* Get the device context */
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiSerialIoProtocolGuid,
                  (VOID **) &SerialIo,
                  Pl2303DriverBinding.DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *ControllerName = (PL2303_FROM_SERIAL_IO (SerialIo))->Name;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
Pl2303GetControllerName2 (
  IN  EFI_COMPONENT_NAME2_PROTOCOL  *This,
  IN  EFI_HANDLE                     ControllerHandle,
  IN  EFI_HANDLE                     ChildHandle      OPTIONAL,
  IN  CHAR8                         *Language,
  OUT CHAR16                       **ControllerName
  )
{
  return Pl2303GetControllerName (NULL, ControllerHandle, ChildHandle, Language, ControllerName);
}

EFI_COMPONENT_NAME_PROTOCOL Pl2303ComponentName = {
  .GetDriverName = Pl2303GetDriverName,
  .GetControllerName = Pl2303GetControllerName,
  .SupportedLanguages = (CHAR8*)"eng"
};

EFI_COMPONENT_NAME2_PROTOCOL Pl2303ComponentName2 = {
  .GetDriverName = Pl2303GetDriverName2,
  .GetControllerName = Pl2303GetControllerName2,
  .SupportedLanguages = (CHAR8*)"en"
};

/* =========================================================================
 * EFI_SERIAL_IO_PROTOCOL implementation
 * ========================================================================= */

/**
  Reset the serial device to its default state and re-apply the current
  line coding and modem-control settings.

  @param[in]  This  EFI_SERIAL_IO_PROTOCOL instance.

  @retval EFI_SUCCESS       Device was reset successfully.
  @retval EFI_DEVICE_ERROR  Hardware reset failed.
**/
EFI_STATUS
EFIAPI
Pl2303SerialReset (
  IN EFI_SERIAL_IO_PROTOCOL  *This
  )
{
  PL2303_PRIVATE_DATA  *Dev;
  EFI_STATUS            Status;

  Dev = PL2303_FROM_SERIAL_IO (This);

  /* Flush the software receive buffer */
  Dev->RxHead      = 0;
  Dev->RxTail      = 0;
  Dev->LineControl = 0;
  Dev->LineStatus  = 0;

  Status = Pl2303Startup (Dev);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  /* Re-apply current line coding */
  Status = Pl2303SerialSetAttributes (
             This,
             Dev->Mode.BaudRate,
             Dev->Mode.ReceiveFifoDepth,
             Dev->Mode.Timeout,
             Dev->Mode.Parity,
             (UINT8)Dev->Mode.DataBits,
             Dev->Mode.StopBits
             );
  return Status;
}

/**
  Set serial-line parameters: baud rate, FIFO depth, timeout, parity,
  data bits and stop bits.

  @param[in]  This             EFI_SERIAL_IO_PROTOCOL instance.
  @param[in]  BaudRate         Requested baud rate (0 = keep current).
  @param[in]  ReceiveFifoDepth Receive FIFO depth (0 = keep current).
  @param[in]  Timeout          Read/write timeout in microseconds (0 = keep).
  @param[in]  Parity           Parity type.
  @param[in]  DataBits         Data bits per character (5–8; 0 = keep current).
  @param[in]  StopBits         Stop-bit count.

  @retval EFI_SUCCESS           Parameters accepted and programmed.
  @retval EFI_INVALID_PARAMETER An unsupported combination was requested.
  @retval EFI_DEVICE_ERROR      USB transfer failed.
**/
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
  )
{
  PL2303_PRIVATE_DATA  *Dev;
  UINT8                 Buf[7];
  UINT32                ActualBaud;
  EFI_STATUS            Status;

  Dev = PL2303_FROM_SERIAL_IO (This);

  /* Apply defaults for zero / default values */
  if (BaudRate         == 0)              { BaudRate         = Dev->Mode.BaudRate;         }
  if (ReceiveFifoDepth == 0)              { ReceiveFifoDepth = Dev->Mode.ReceiveFifoDepth;  }
  if (Timeout          == 0)              { Timeout          = Dev->Mode.Timeout;           }
  if (DataBits         == 0)              { DataBits         = (UINT8)Dev->Mode.DataBits;   }
  if (Parity           == DefaultParity)  { Parity           = Dev->Mode.Parity;            }
  if (StopBits         == DefaultStopBits){ StopBits         = Dev->Mode.StopBits;          }

  /* Validate */
  if ((DataBits < 5) || (DataBits > 8)) {
    return EFI_INVALID_PARAMETER;
  }
  if ((StopBits == OneFiveStopBits) && (DataBits != 5)) {
    return EFI_INVALID_PARAMETER;
  }

  /* Read the current line-coding from the device as a starting point */
  Status = Pl2303GetLineRequest (Dev, Buf);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  /* --- Baud rate (bytes 0-3) --- */
  ActualBaud = Pl2303EncodeBaudRate (Dev, (UINT32)BaudRate, &Buf[0]);

  /* --- Stop bits (byte 4) ---
   *   0 = 1 stop bit, 1 = 1.5 stop bits (5 data only), 2 = 2 stop bits
   */
  switch (StopBits) {
  case OneStopBit:
    Buf[4] = 0;
    break;
  case OneFiveStopBits:
    Buf[4] = 1;
    break;
  case TwoStopBits:
    Buf[4] = 2;
    break;
  default:
    Buf[4] = 0;
    break;
  }

  /* --- Parity (byte 5) ---
   *   0=none, 1=odd, 2=even, 3=mark, 4=space
   */
  switch (Parity) {
  case NoParity:
    Buf[5] = 0;
    break;
  case OddParity:
    Buf[5] = 1;
    break;
  case EvenParity:
    Buf[5] = 2;
    break;
  case MarkParity:
    Buf[5] = 3;
    break;
  case SpaceParity:
    Buf[5] = 4;
    break;
  default:
    Buf[5] = 0;
    break;
  }

  /* --- Data bits (byte 6) --- */
  Buf[6] = DataBits;

  /* Only send to device if anything changed (avoids dropping bytes) */
  if (CompareMem (Buf, Dev->LineSettings, 7) != 0) {
    Status = Pl2303SetLineRequest (Dev, Buf);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
  }

  /* Apply hardware flow-control setting */
  if (Dev->HwFlowControl) {
    if (Dev->Quirks & PL2303_QUIRK_LEGACY) {
      Pl2303UpdateReg (Dev, 0, PL2303_FLOWCTRL_MASK, 0x40);
    } else if (Dev->Type == TYPE_HXN) {
      Pl2303UpdateReg (Dev, PL2303_HXN_FLOWCTRL_REG,
                       PL2303_HXN_FLOWCTRL_MASK, PL2303_HXN_FLOWCTRL_RTS_CTS);
    } else {
      Pl2303UpdateReg (Dev, 0, PL2303_FLOWCTRL_MASK, 0x60);
    }
  } else {
    if (Dev->Type == TYPE_HXN) {
      Pl2303UpdateReg (Dev, PL2303_HXN_FLOWCTRL_REG,
                       PL2303_HXN_FLOWCTRL_MASK, PL2303_HXN_FLOWCTRL_NONE);
    } else {
      Pl2303UpdateReg (Dev, 0, PL2303_FLOWCTRL_MASK, 0);
    }
  }

  /* Update mode record */
  Dev->Mode.BaudRate         = ActualBaud;
  Dev->Mode.ReceiveFifoDepth = ReceiveFifoDepth;
  Dev->Mode.Timeout          = Timeout;
  Dev->Mode.Parity           = Parity;
  Dev->Mode.DataBits         = DataBits;
  Dev->Mode.StopBits         = StopBits;

  return EFI_SUCCESS;
}

/**
  Set the control signals (DTR, RTS, hardware flow control).

  @param[in]  This     EFI_SERIAL_IO_PROTOCOL instance.
  @param[in]  Control  Bitmask of EFI_SERIAL_* control flags.

  @retval EFI_SUCCESS           Control signals updated.
  @retval EFI_UNSUPPORTED       An unsupported control bit was set.
  @retval EFI_DEVICE_ERROR      USB transfer failed.
**/
EFI_STATUS
EFIAPI
Pl2303SerialSetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT32                   Control
  )
{
  PL2303_PRIVATE_DATA  *Dev;
  UINT8                 NewControl;
  BOOLEAN               HwFlow;
  EFI_STATUS            Status;

  Dev = PL2303_FROM_SERIAL_IO (This);

  /* Loopback modes are not supported in hardware */
  if ((Control & (EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE |
                  EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE)) != 0) {
    return EFI_UNSUPPORTED;
  }

  NewControl = 0;
  if ((Control & EFI_SERIAL_DATA_TERMINAL_READY) != 0) {
    NewControl |= CONTROL_DTR;
  }
  if ((Control & EFI_SERIAL_REQUEST_TO_SEND) != 0) {
    NewControl |= CONTROL_RTS;
  }

  HwFlow = (BOOLEAN)((Control & EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE) != 0);

  Dev->LineControl  = NewControl;
  Dev->HwFlowControl = HwFlow;

  Status = Pl2303SetControlLines (Dev);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  /* Re-apply flow-control register whenever the setting changes */
  if (HwFlow) {
    if (Dev->Quirks & PL2303_QUIRK_LEGACY) {
      Pl2303UpdateReg (Dev, 0, PL2303_FLOWCTRL_MASK, 0x40);
    } else if (Dev->Type == TYPE_HXN) {
      Pl2303UpdateReg (Dev, PL2303_HXN_FLOWCTRL_REG,
                       PL2303_HXN_FLOWCTRL_MASK, PL2303_HXN_FLOWCTRL_RTS_CTS);
    } else {
      Pl2303UpdateReg (Dev, 0, PL2303_FLOWCTRL_MASK, 0x60);
    }
  } else {
    if (Dev->Type == TYPE_HXN) {
      Pl2303UpdateReg (Dev, PL2303_HXN_FLOWCTRL_REG,
                       PL2303_HXN_FLOWCTRL_MASK, PL2303_HXN_FLOWCTRL_NONE);
    } else {
      Pl2303UpdateReg (Dev, 0, PL2303_FLOWCTRL_MASK, 0);
    }
  }

  return EFI_SUCCESS;
}

/**
  Return the current state of the serial control signals.

  If an interrupt-IN endpoint is present, a synchronous poll is issued to
  refresh the cached modem-status byte.  The input-buffer-empty flag
  reflects whether the software receive buffer contains unread data.

  @param[in]   This     EFI_SERIAL_IO_PROTOCOL instance.
  @param[out]  Control  Receives the EFI_SERIAL_* bitmask.

  @retval EFI_SUCCESS  Control updated.
**/
EFI_STATUS
EFIAPI
Pl2303SerialGetControl (
  IN  EFI_SERIAL_IO_PROTOCOL  *This,
  OUT UINT32                  *Control
  )
{
  PL2303_PRIVATE_DATA  *Dev;
  UINT32                Result;

  Dev = PL2303_FROM_SERIAL_IO (This);

  Result = 0;

  /* Output signals */
  if ((Dev->LineControl & CONTROL_DTR) != 0) {
    Result |= EFI_SERIAL_DATA_TERMINAL_READY;
  }
  if ((Dev->LineControl & CONTROL_RTS) != 0) {
    Result |= EFI_SERIAL_REQUEST_TO_SEND;
  }

  /* Input (modem-status) signals */
  if ((Dev->LineStatus & UART_CTS)  != 0) { Result |= EFI_SERIAL_CLEAR_TO_SEND;   }
  if ((Dev->LineStatus & UART_DSR)  != 0) { Result |= EFI_SERIAL_DATA_SET_READY;   }
  if ((Dev->LineStatus & UART_RING) != 0) { Result |= EFI_SERIAL_RING_INDICATE;    }
  if ((Dev->LineStatus & UART_DCD)  != 0) { Result |= EFI_SERIAL_CARRIER_DETECT;   }

  /* Software state */
  if (RxAvail (Dev) == 0) {
    Result |= EFI_SERIAL_INPUT_BUFFER_EMPTY;
  }
  Result |= EFI_SERIAL_OUTPUT_BUFFER_EMPTY;  /* no TX buffering in this driver */

  if (Dev->HwFlowControl) {
    Result |= EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE;
  }

  *Control = Result;
  return EFI_SUCCESS;
}

/**
  Transmit data over the serial interface.

  @param[in]      This        EFI_SERIAL_IO_PROTOCOL instance.
  @param[in,out]  BufferSize  On entry: bytes to send.  On exit: bytes sent.
  @param[in]      Buffer      Data to transmit.

  @retval EFI_SUCCESS       All bytes were sent.
  @retval EFI_DEVICE_ERROR  A USB bulk-OUT transfer failed.
  @retval EFI_TIMEOUT       Not all bytes could be sent within the timeout.
**/
EFI_STATUS
EFIAPI
Pl2303SerialWrite (
  IN     EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN                   *BufferSize,
  IN     VOID                    *Buffer
  )
{
  PL2303_PRIVATE_DATA  *Dev;
  UINT8                *Ptr;
  UINTN                 Remaining;
  UINTN                 Chunk;
  UINTN                 Sent;
  UINT32                UsbStatus;
  EFI_STATUS            Status;
  UINTN                 TimeoutMs;

  Dev       = PL2303_FROM_SERIAL_IO (This);
  Ptr       = (UINT8 *)Buffer;
  Remaining = *BufferSize;
  Sent      = 0;

  TimeoutMs = Dev->Mode.Timeout / 1000;
  if (TimeoutMs == 0) {
    TimeoutMs = 100;
  }
  if (TimeoutMs > 5000) {
    TimeoutMs = 5000;
  }

  while (Remaining > 0) {
    Chunk = (Remaining > PL2303_MAX_XFER_SIZE) ? PL2303_MAX_XFER_SIZE : Remaining;

    Status = Dev->UsbIo->UsbBulkTransfer (
                           Dev->UsbIo,
                           Dev->BulkOutAddr,
                           Ptr,
                           &Chunk,
                           (UINTN)TimeoutMs,
                           &UsbStatus
                           );
    if (EFI_ERROR (Status)) {
      *BufferSize = Sent;
      return (Status == EFI_TIMEOUT) ? EFI_TIMEOUT : EFI_DEVICE_ERROR;
    }

    Ptr       += Chunk;
    Sent      += Chunk;
    Remaining -= Chunk;
  }

  *BufferSize = Sent;
  return EFI_SUCCESS;
}

/**
  Receive data from the serial interface.

  Attempts to fill the caller's buffer within Mode->Timeout microseconds.
  Returns EFI_TIMEOUT if the buffer could not be completely filled in time,
  with *BufferSize updated to the number of bytes actually received.

  @param[in]      This        EFI_SERIAL_IO_PROTOCOL instance.
  @param[in,out]  BufferSize  On entry: bytes requested.  On exit: bytes received.
  @param[out]     Buffer      Receives the data.

  @retval EFI_SUCCESS   All requested bytes were received.
  @retval EFI_TIMEOUT   Fewer bytes were received before the timeout expired.
**/
EFI_STATUS
EFIAPI
Pl2303SerialRead (
  IN     EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN                   *BufferSize,
  OUT    VOID                    *Buffer
  )
{
  PL2303_PRIVATE_DATA  *Dev;
  UINT8                *Ptr;
  UINTN                 Received;
  UINTN                 Needed;
  UINT8                 Tmp[PL2303_MAX_XFER_SIZE];
  UINTN                 Len;
  UINT32                UsbStatus;
  EFI_STATUS            Status;
  UINTN                 TotalTimeoutMs;
  UINTN                 ElapsedMs;
  UINTN                 PerXferMs;

  Dev       = PL2303_FROM_SERIAL_IO (This);
  Ptr       = (UINT8 *)Buffer;
  Needed    = *BufferSize;
  Received  = 0;

  TotalTimeoutMs = Dev->Mode.Timeout / 1000;
  if (TotalTimeoutMs == 0) {
    TotalTimeoutMs = 1;
  }
  if (TotalTimeoutMs > 10000) {
    TotalTimeoutMs = 10000;
  }
  PerXferMs  = 100;
  ElapsedMs  = 0;

  /* Drain any bytes already in the software receive buffer */
  Received += RxDequeue (Dev, Ptr + Received, Needed - Received);

  /* Issue bulk-IN transfers until the buffer is full or we time out */
  while (Received < Needed) {
    UINTN  Available;

    Available = TotalTimeoutMs - ElapsedMs;
    if (Available == 0) {
      break;
    }
    if (Available > PerXferMs) {
      Available = PerXferMs;
    }

    Len    = sizeof (Tmp);
    Status = Dev->UsbIo->UsbBulkTransfer (
                           Dev->UsbIo,
                           Dev->BulkInAddr,
                           Tmp,
                           &Len,
                           (UINTN)Available,
                           &UsbStatus
                           );
    ElapsedMs += Available;

    if (!EFI_ERROR (Status) && (Len > 0)) {
      /* Buffer incoming data and immediately consume what the caller needs */
      RxEnqueue (Dev, Tmp, Len);
      Received += RxDequeue (Dev, Ptr + Received, Needed - Received);
    }
  }

  *BufferSize = Received;
  return (Received < Needed) ? EFI_TIMEOUT : EFI_SUCCESS;
}

/* =========================================================================
 * EFI_DRIVER_BINDING_PROTOCOL implementation
 * ========================================================================= */

/**
  Test whether this driver can manage the given controller handle.

  @param[in]  This                  Driver-binding protocol instance.
  @param[in]  ControllerHandle      Handle to test.
  @param[in]  RemainingDevicePath   Unused.

  @retval EFI_SUCCESS     The device is a supported PL2303 adapter.
  @retval EFI_UNSUPPORTED The device is not supported.
**/
EFI_STATUS
EFIAPI
Pl2303DriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                    ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS           Status;
  EFI_USB_IO_PROTOCOL  *UsbIo;
  UINTN                Quirks;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Pl2303IsDeviceSupported (UsbIo, &Quirks)
           ? EFI_SUCCESS
           : EFI_UNSUPPORTED;

  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiUsbIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );
  return Status;
}

/**
  Start managing the PL2303 device on ControllerHandle.

  Opens EFI_USB_IO_PROTOCOL, detects chip type, discovers endpoints,
  runs the power-on initialisation sequence, and installs
  EFI_SERIAL_IO_PROTOCOL on the controller handle.

  @param[in]  This                  Driver-binding protocol instance.
  @param[in]  ControllerHandle      Handle of the PL2303 USB interface.
  @param[in]  RemainingDevicePath   Unused.

  @retval EFI_SUCCESS           Driver started successfully.
  @retval EFI_UNSUPPORTED       Device not supported (should not happen after Supported).
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
  @retval EFI_DEVICE_ERROR      Hardware initialisation failed.
  @retval other                 An EFI service call failed.
**/
EFI_STATUS
EFIAPI
Pl2303DriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                    ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_USB_DEVICE_DESCRIPTOR  Desc;
  EFI_STATUS                 Status;
  EFI_USB_IO_PROTOCOL       *UsbIo;
  PL2303_PRIVATE_DATA       *Dev;
  UINTN                      Quirks;
  UINTN                      Size;
  INT32                      Type;
  CHAR16                    *ProductString = NULL;

  /* Open USB I/O */
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Confirm device is in the supported-device table */
  if (!Pl2303IsDeviceSupported (UsbIo, &Quirks)) {
    Status = EFI_UNSUPPORTED;
    goto CloseUsbIo;
  }

  /* Detect chip type */
  Type = Pl2303DetectType (UsbIo);
  if (Type < 0) {
    DEBUG ((DEBUG_ERROR, "PL2303: Failed to detect chip type\n"));
    Status = EFI_DEVICE_ERROR;
    goto CloseUsbIo;
  }

  /* Allocate zeroed private context */
  Dev = AllocateZeroPool (sizeof (PL2303_PRIVATE_DATA));
  if (Dev == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CloseUsbIo;
  }

  Dev->Signature    = PL2303_PRIVATE_DATA_SIGNATURE;
  Dev->UsbIo        = UsbIo;
  Dev->Type         = (PL2303_TYPE)Type;
  Dev->Quirks       = Quirks | Pl2303TypeData[Type].Quirks;

  if ((Dev->Type == TYPE_HXD) && Pl2303IsHxdClone (UsbIo)) {
    Dev->Quirks |= PL2303_QUIRK_NO_BREAK_GETLINE;
  }

  /* Populate the device name */
  if (UsbIo->UsbGetDeviceDescriptor (UsbIo, &Desc) == EFI_SUCCESS && Desc.StrProduct != 0) {
    UsbIo->UsbGetStringDescriptor (UsbIo, 0x0409, Desc.StrProduct, &ProductString);
    Size = 64 + ((ProductString != NULL) ? 2 * StrLen(ProductString) : 0);
    Dev->Name = AllocateZeroPool(Size);
    if (Dev->Name != NULL) {
      UnicodeSPrint(Dev->Name, Size, L"PL2303%a %s (%04x:%04x)",
        Pl2303TypeData[Dev->Type].TypeName,
        (ProductString != NULL) ? ProductString : L"",
        Desc.IdVendor,
        Desc.IdProduct
        );
    }
    FreePool(ProductString);
  }

  DEBUG ((DEBUG_INFO, "PL2303: Detected type %a, quirks 0x%lx\n",
          Pl2303TypeData[Dev->Type].TypeName, (UINT64)Dev->Quirks));

  /* Discover bulk-in / bulk-out / interrupt-in endpoints */
  Status = Pl2303DiscoverEndpoints (Dev);
  if (EFI_ERROR (Status)) {
    goto FreeDev;
  }

  /* Initialise EFI_SERIAL_IO_PROTOCOL function table */
  Dev->SerialIo.Revision      = SERIAL_IO_INTERFACE_REVISION;
  Dev->SerialIo.Reset         = Pl2303SerialReset;
  Dev->SerialIo.SetAttributes = Pl2303SerialSetAttributes;
  Dev->SerialIo.SetControl    = Pl2303SerialSetControl;
  Dev->SerialIo.GetControl    = Pl2303SerialGetControl;
  Dev->SerialIo.Write         = Pl2303SerialWrite;
  Dev->SerialIo.Read          = Pl2303SerialRead;
  Dev->SerialIo.Mode          = &Dev->Mode;

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
    goto FreeDev;
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
  Status = Pl2303SerialSetAttributes (
             &Dev->SerialIo,
             Dev->Mode.BaudRate,
             Dev->Mode.ReceiveFifoDepth,
             Dev->Mode.Timeout,
             Dev->Mode.Parity,
             (UINT8)Dev->Mode.DataBits,
             Dev->Mode.StopBits
             );
  if (EFI_ERROR (Status)) {
    goto FreeDev;
  }

  /* Install EFI_SERIAL_IO_PROTOCOL on the controller handle */
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ControllerHandle,
                  &gEfiSerialIoProtocolGuid,
                  &Dev->SerialIo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto FreeDev;
  }

  return EFI_SUCCESS;

FreeDev:
  FreePool (Dev);
CloseUsbIo:
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiUsbIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );
  return Status;
}

/**
  Stop managing the PL2303 device on ControllerHandle.

  Uninstalls EFI_SERIAL_IO_PROTOCOL, closes EFI_USB_IO_PROTOCOL, and
  frees the driver private context.

  @param[in]  This               Driver-binding protocol instance.
  @param[in]  ControllerHandle   Handle being stopped.
  @param[in]  NumberOfChildren   Unused (this driver creates no child handles).
  @param[in]  ChildHandleBuffer  Unused.

  @retval EFI_SUCCESS  Driver stopped successfully.
  @retval other        An EFI service call failed.
**/
EFI_STATUS
EFIAPI
Pl2303DriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                    ControllerHandle,
  IN UINTN                         NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS              Status;
  EFI_SERIAL_IO_PROTOCOL *SerialIo;
  PL2303_PRIVATE_DATA    *Dev;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiSerialIoProtocolGuid,
                  (VOID **)&SerialIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Dev = PL2303_FROM_SERIAL_IO (SerialIo);

  /* Uninstall the serial protocol */
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  ControllerHandle,
                  &gEfiSerialIoProtocolGuid,
                  SerialIo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Release USB I/O */
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiUsbIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );

  FreePool (Dev->Name);
  FreePool (Dev);
  return EFI_SUCCESS;
}

/* =========================================================================
 * Driver entry point
 * ========================================================================= */

/**
  Install the EFI_DRIVER_BINDING_PROTOCOL on ImageHandle.

  @param[in]  ImageHandle  Handle of this driver image.
  @param[in]  SystemTable  Pointer to the EFI system table.

  @retval EFI_SUCCESS  Protocol installed successfully.
  @retval other        Installation failed.
**/
EFI_STATUS
EFIAPI
Pl2303DxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  Pl2303DriverBinding.DriverBindingHandle = ImageHandle;
  Pl2303DriverBinding.ImageHandle         = ImageHandle;

  return gBS->InstallMultipleProtocolInterfaces (
                &Pl2303DriverBinding.DriverBindingHandle,
                &gEfiDriverBindingProtocolGuid,
                &Pl2303DriverBinding,
                &gEfiComponentNameProtocolGuid,
                &Pl2303ComponentName,
                &gEfiComponentName2ProtocolGuid,
                &Pl2303ComponentName2,
                NULL
                );
}
