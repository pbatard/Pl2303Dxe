/** @file
  EFI_SERIAL_IO_PROTOCOL Test Application

  Copyright (c) 2012, Ashley DeSimone

  Portions Copyright (c) 2009 - 2013, Intel Corporation. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD
  License which accompanies this distribution.  The full text of the license may
  be found at http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SerialIo.h>

#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/HandleParsingLib.h>
#include <Protocol/DevicePathToText.h>

#define CHKEFIERR(ret,msg) if(EFI_ERROR(ret)) { \
  SystemTable->ConOut->SetAttribute(SystemTable->ConOut, EFI_RED); \
  Print(msg); return ret; }

/**
  Gets the name of the device specified by the device handle.

  This function gets the user-readable name of the device specified by the
  device handle. If no user-readable name could be generated, then
  *BestDeviceName will be NULL and EFI_NOT_FOUND will be returned.

  If EFI_DEVICE_NAME_USE_COMPONENT_NAME is set, then the function will return
  the device's name using the EFI_COMPONENT_NAME2_PROTOCOL, if present on
  DeviceHandle.

  If EFI_DEVICE_NAME_USE_DEVICE_PATH is set, then the function will return the
  device's name using the EFI_DEVICE_PATH_PROTOCOL, if present on DeviceHandle.
  If both EFI_DEVICE_NAME_USE_COMPONENT_NAME and
  EFI_DEVICE_NAME_USE_DEVICE_PATH are set, then
  EFI_DEVICE_NAME_USE_COMPONENT_NAME will have higher priority.

  @param DeviceHandle[in]            The handle of the device.
  @param Flags[in]                   Determines the possible sources of component
                                     names.
                                     Valid bits are:
                                       EFI_DEVICE_NAME_USE_COMPONENT_NAME
                                       EFI_DEVICE_NAME_USE_DEVICE_PATH
  @param Language[in]                A pointer to the language specified for the
                                     device name, in the same format as
                                     described in the UEF specification,
                                     Appendix M.
  @param BestDeviceName[out]         On return, points to the callee-allocated
                                     NULL-terminated name of the device. If no
                                     device name could be found, points to NULL.
                                     The name must be freed by the caller.

  @retval EFI_SUCCESS                Get the name successfully.
  @retval EFI_NOT_FOUND              Fail to get the device name.
  @retval EFI_INVALID_PARAMETER      Flags did not have a valid bit set.
  @retval EFI_INVALID_PARAMETER      BestDeviceName was NULL.
  @retval EFI_INVALID_PARAMETER      DeviceHandle was NULL.
**/
EFI_STATUS
EFIAPI
EfiShellGetDeviceName (
  IN EFI_HANDLE DeviceHandle,
  IN EFI_SHELL_DEVICE_NAME_FLAGS Flags,
  IN CHAR8 *Language,
  OUT CHAR16 **BestDeviceName
  );


/**
  Prints the attributes of the serial device to the screen.
  
  The attributes printed by this function are:
    Data Bits
    Baudrate
    Stop Bits
    Parity

  @param  SerialDevice[in]  Instance of the Serial Io Protocol

**/
VOID
EFIAPI
DisplaySerialDeviceAttributes (
  IN EFI_SERIAL_IO_PROTOCOL   *SerialDevice
  );

/**
  Displays the current enable/disable state of the control bits.

  @param  Control[in]      The current control value.
**/
VOID
EFIAPI
DisplaySerialDeviceControlBits (
  IN UINT32       Control
  );

/**
  Displays the main menu of the test application.

**/
VOID
EFIAPI
DisplayMainTestMenu (
  VOID
  );

/**
  Displays the selection menu for parity options.
**/
VOID
EFIAPI
DisplayParityMenu (
  VOID
  );

/**
  Displays the selection menu for stop bit options.

**/
VOID
EFIAPI
DisplayStopBitsMenu (
  VOID
  );

/**
  Displays the selection menu for baudrate options.

**/
VOID
EFIAPI
DisplayBaudRateMenu (
  VOID
  );


/**
  Displays the selection menu for data options.

**/
VOID
EFIAPI
DisplayDataBitsMenu (
  VOID
  );

/**
  Displays the attributes that will be set on the Usb Serial Device.

  @param  BaudRate[in]    The baudrate to be set on the device.
  @param  Parity[in]      The parity to be set on the device.
  @param  DataBits[in]    The data bits value to be set on the device.
  @param  StopBits[in]    The stop bits value to be set on the device.

**/
VOID
EFIAPI
DisplayAttributesToBeSet (
  IN UINT64               BaudRate,
  IN EFI_PARITY_TYPE      Parity,
  IN UINT8                DataBits,
  IN EFI_STOP_BITS_TYPE   StopBits
  );

/**
  Displays the selection menu for setting the Usb Serial Device Attributes.

**/
VOID
EFIAPI
DisplaySetAttributesMenu (
  VOID
  );

/**
  Sets the attributes on the device based on user input.

  This function prompts the user with options for setting the attributes on the
  device. Once a selection has been made the selections are converted to the
  formats expected but the SetAttributes method of the Serial IO Protocol.
  SetAttributes is then called. At this time Timeout and ReceiveFifoDepth values
  cannot be set.

  @param  SerialIo[in]      The current instance of the Serial Io Protocol.

  @retval EFI_SUCCESS       The user was able to exit the test successfully.

**/
EFI_STATUS
EFIAPI
SetAttributesTest (
  IN EFI_SERIAL_IO_PROTOCOL   *SerialIo
  );

/**
  Displays the enable disable menu that is shown when the user is setting the
  control bits.

**/
VOID
EFIAPI
DisplayEnableDisableMenu (
  VOID
  );

/**
  Displays which control bits can be set and prompts the user for a selection.

**/
VOID
EFIAPI
DisplayControlBitMenu (
  VOID
  );

/**
  Allows the user to alter the enable/disable status of Usb Serial Device's
  control bits.

  Prompts the user to select a control bit and the enable/disable status to be
  set. Converts these values to the format expected by the SetControl method of
  the SerialIO protocol. Calls SetControl to set the user selected values.

  @param  SerialIo[in]      The current instance of the Serial Io Protocol.

  @retval EFI_SUCCESS       The user was able to exit from the test successfully.

**/
EFI_STATUS 
EFIAPI
SetControlBitsTest (
  IN EFI_SERIAL_IO_PROTOCOL   *SerialIo
  );