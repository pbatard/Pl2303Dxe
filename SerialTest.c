/** @file
  EFI_SERIAL_IO_PROTOCOL Test Application

  Copyright (c) 2012, Ashley DeSimone
  
  Portions Copyright (c) 2009 - 2013, Intel Corporation. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD 
  License which accompanies this distribution. The full text of the license may
  be found at http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "SerialTest.h"

/**
  The main function of the serial test application.

  @param  ImageHandle[in]
  @param  SystemTable[in]

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN                  BufferSize;
  EFI_HANDLE             *Buffer;
  EFI_STATUS             Status;
  INTN                   i;
  EFI_SERIAL_IO_PROTOCOL *Serial;
  UINTN                  StringLength;
  CHAR16                 *DeviceName;
  UINTN                  Index;
  EFI_INPUT_KEY          Keystroke;
  BOOLEAN                ExitLoop;
  UINT32                 Control;
  UINT8                  DataBuffer[1024];
  UINTN                  DataBufferSize;

  Print (L"Serial Test Application\n");
  Print (L"Copyright 2012 (c) Ashley DeSimone\n");
  Print (L"Portions Copyright (c) 2009 - 2013, Intel Corporation. All rights reserved.\n");
  Print (L"\n");

  //
  // Figure out how big of an array is needed.
  //
  Index      = 0;
  BufferSize = 0;
  Buffer     = NULL;

  Status = gBS->LocateHandle (
                  ByProtocol,
                  &gEfiSerialIoProtocolGuid,
                  NULL,
                  &BufferSize,
                  Buffer
                  );

  //
  // Check to make sure there is at least 1 handle in the system that implements
  // our protocol
  //
  if(Status != EFI_BUFFER_TOO_SMALL) {
    Print(L"There are no serial ports attached to the system.\n");
    return EFI_SUCCESS;
  }

  Print (L"Found %d serial ports\n", (BufferSize / sizeof (EFI_HANDLE)));

  Buffer = AllocateZeroPool (sizeof (EFI_HANDLE) * BufferSize);
  if (Buffer == NULL) {
    Print (L"Out of memory\n");
    return EFI_SUCCESS;
  }

  Status = gBS->LocateHandle (
                  ByProtocol,
                  &gEfiSerialIoProtocolGuid,
                  NULL,
                  &BufferSize,
                  Buffer
                  );
  CHKEFIERR (Status, L"Unexpected error getting handles\n");

  Print (L"Select Serial Port to Open:\n");

  //
  // Get and print the name of all of the devices in Buffer.
  //
  for (i = 0; i < (BufferSize / (INTN)sizeof(EFI_HANDLE)); i++) {
    Status = EfiShellGetDeviceName ( 
               Buffer[i],
               EFI_DEVICE_NAME_USE_COMPONENT_NAME | EFI_DEVICE_NAME_USE_DEVICE_PATH,
               "en",
               &DeviceName
               );

    if (!EFI_ERROR(Status)) {
      Print (L"%d. %s\n", (i + 1), DeviceName);
      FreePool (DeviceName);
    } else {
      Print (L"%d. <Name Unknown>\n", (i + 1));
    }
  }

  //
  // Get the users response and if valid open the selected port.
  //
  Keystroke.UnicodeChar = 0;
  while (Keystroke.UnicodeChar < 48 || Keystroke.UnicodeChar > 57) {
    Status = EFI_INVALID_PARAMETER;
    while (EFI_ERROR (Status)) {
      Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
    }

    gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);

    if (Keystroke.UnicodeChar < 49 || Keystroke.UnicodeChar > 57) {
      Print (L"Invalid Port Selection\n");
    }

    Index = Keystroke.UnicodeChar - 49;
    if (Index >= (UINTN)(BufferSize / (INTN)sizeof(EFI_HANDLE))) {
      Print (L"Invalid Port Selection\n");
      Keystroke.UnicodeChar = 0;
    }
  }

  Status = gBS->OpenProtocol (
                  Buffer[Index],
                  &gEfiSerialIoProtocolGuid,
                  (void**)&Serial,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    Print (L"Unexpected error accessing protocol\n");
    return EFI_SUCCESS;
  }

  gST->ConOut->ClearScreen (gST->ConOut);
  DisplayMainTestMenu ();

  //
  // Get the users response to the main test menu. Display the appropriate data /
  // submenu and perform and appopriate tests. Once the user finishes with their
  // selection display the main test menu again.
  //
  Keystroke.UnicodeChar = 0;
  Keystroke.ScanCode = 0x00;
  while (Keystroke.ScanCode != 0x17) {
    while (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x35) {
      Status = EFI_INVALID_PARAMETER;
      while (EFI_ERROR (Status)) {
        Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
      }
      Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
      if (Keystroke.ScanCode == 0x17) {
        Print (L"Exiting....\n");
        break;
      }
      if (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x35) {
        Print (L"Invalid Selection\n");
        Keystroke.UnicodeChar = 0;
      }
    }
    if (Keystroke.ScanCode == 0x17) {
      //
      // The user has selected the exit option.
      //
      gBS->CloseProtocol ( 
             Buffer[Index],
             &gEfiSerialIoProtocolGuid,
             ImageHandle,
             NULL
             );
      FreePool (Buffer);
      Buffer = NULL;

      return EFI_SUCCESS;
    }

    if (Keystroke.UnicodeChar == 0x31) {
      //
      // The user has selected the view attributes option.
      //
      DisplaySerialDeviceAttributes(Serial);
    }
    if (Keystroke.UnicodeChar == 0x32) {
      //
      // The user has selected the view control bits option.
      //
      Status = Serial->GetControl (Serial, &Control);
      DisplaySerialDeviceControlBits(Control);
    }
    if (Keystroke.UnicodeChar == 0x33) {
      //
      // The user has selected the write to serial port option.
      //
      gST->ConOut->ClearScreen (gST->ConOut);
      DisplaySerialDeviceAttributes(Serial);
      Print (L"SERIAL PORT OPEN\nType to send data, Press ESC to quit.\n");

      ExitLoop = FALSE;
      while (!ExitLoop) {
        //
        // Check if there is data waiting. If so read it and print to screen
        //
        Status = Serial->GetControl (Serial, &Control);
        if (EFI_ERROR (Status)) {
          Print (L"Error Getting Serial Device Control Bits\n");
          ExitLoop = TRUE;
          break;
        }
        if ((Control & EFI_SERIAL_INPUT_BUFFER_EMPTY) == 0) {
          DataBufferSize = 1023;
          Status = Serial->Read (Serial, &DataBufferSize, DataBuffer);
          if (Status == EFI_TIMEOUT || !EFI_ERROR (Status)) {
            DataBuffer[DataBufferSize] = '\0';
            Print(L"%a", DataBuffer);
          } else {
            Print (L"Error reading from serial device\n");
            ExitLoop = TRUE;
            break;
          }
        }
        //
        // Check if there is a keystroke waiting to be sent
        //
        while (!EFI_ERROR (Status)) {
          Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
          if (!EFI_ERROR (Status)) {
            if (Keystroke.ScanCode == 0x17) {
              ExitLoop = TRUE;
              break;
            }
            if (Keystroke.UnicodeChar == 0x0) {
              continue;
            }
            StringLength = 1;
            DataBuffer[0] = (UINT8)Keystroke.UnicodeChar;
            Serial->Write (Serial, &StringLength, DataBuffer);
          }
        }
      }
      Keystroke.ScanCode = 0x0;
    }
    if (Keystroke.UnicodeChar == 0x34) {
      //
      // The user has selected the set attributes option.
      //
      Status = SetAttributesTest (Serial);
    }
    if (Keystroke.UnicodeChar == 0x35) {
      //
      // The user has selected the set control bits option.
      //
      Status = SetControlBitsTest (Serial);
    }
    Keystroke.UnicodeChar = 0x0;
    Keystroke.ScanCode    = 0x0;
    DisplayMainTestMenu ();
  }

  gBS->CloseProtocol ( 
         Buffer[Index],
         &gEfiSerialIoProtocolGuid,
         ImageHandle,
         NULL
         );
  FreePool (Buffer);
  Buffer = NULL;

  return EFI_SUCCESS;
}

//
// Function definitions
//

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
  IN  EFI_HANDLE                   DeviceHandle,
  IN  EFI_SHELL_DEVICE_NAME_FLAGS  Flags,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **BestDeviceName
  )
{
  EFI_STATUS                        Status;
  EFI_COMPONENT_NAME2_PROTOCOL      *CompName2;
  EFI_DEVICE_PATH_TO_TEXT_PROTOCOL  *DevicePathToText;
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
  EFI_HANDLE                        *HandleList;
  UINTN                             HandleCount;
  UINTN                             LoopVar;
  CHAR16                            *DeviceNameToReturn;
  CHAR8                             *Lang;
  CHAR8                             *TempChar;

  UINTN                             ParentControllerCount;
  EFI_HANDLE                        *ParentControllerBuffer;
  UINTN                             ParentDriverCount;
  EFI_HANDLE                        *ParentDriverBuffer;

  if (BestDeviceName == NULL ||
      DeviceHandle   == NULL
     ){
    return (EFI_INVALID_PARAMETER);
  }

  //
  // make sure one of the 2 supported bits is on
  //
  if (((Flags & EFI_DEVICE_NAME_USE_COMPONENT_NAME) == 0) &&
      ((Flags & EFI_DEVICE_NAME_USE_DEVICE_PATH) == 0)) {
    return (EFI_INVALID_PARAMETER);
  }

  DeviceNameToReturn  = NULL;
  *BestDeviceName     = NULL;
  HandleList          = NULL;
  HandleCount         = 0;
  Lang                = NULL;

  if ((Flags & EFI_DEVICE_NAME_USE_COMPONENT_NAME) != 0) {
    Status = ParseHandleDatabaseByRelationship (
               NULL,
               DeviceHandle,
               HR_DRIVER_BINDING_HANDLE|HR_DEVICE_DRIVER,
               &HandleCount,
               &HandleList
               );
    for (LoopVar = 0; LoopVar < HandleCount; LoopVar++) {
      //
      // Go through those handles until we get one that passes for
      // GetComponentName
      //
      Status = gBS->OpenProtocol (
                      HandleList[LoopVar],
                      &gEfiComponentName2ProtocolGuid,
                      (VOID**)&CompName2,
                      gImageHandle,
                      NULL,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );
      if (EFI_ERROR (Status)) {
        Status = gBS->OpenProtocol(
                        HandleList[LoopVar],
                        &gEfiComponentNameProtocolGuid,
                        (VOID**)&CompName2,
                        gImageHandle,
                        NULL,
                        EFI_OPEN_PROTOCOL_GET_PROTOCOL
                        );
      }

      if (EFI_ERROR(Status)) {
        continue;
      }
      if (Language == NULL) {
        Lang = AllocateZeroPool (AsciiStrSize(CompName2->SupportedLanguages));
        if (Lang == NULL) {
          return (EFI_OUT_OF_RESOURCES);
        }
        AsciiStrCpyS (Lang, AsciiStrSize(CompName2->SupportedLanguages), CompName2->SupportedLanguages);
        TempChar = AsciiStrStr (Lang, ";");
        if (TempChar != NULL){
          *TempChar = CHAR_NULL;
        }
      } else {
        Lang = AllocateZeroPool (AsciiStrSize (Language));
        if (Lang == NULL) {
          return (EFI_OUT_OF_RESOURCES);
        }
        AsciiStrCpyS (Lang, AsciiStrSize (Language), Language);
      }
      Status = CompName2->GetControllerName (
                            CompName2,
                            DeviceHandle,
                            NULL,
                            Lang,
                            &DeviceNameToReturn
                            );
      FreePool (Lang);
      Lang = NULL;
      if (!EFI_ERROR(Status) && DeviceNameToReturn != NULL) {
        break;
      }
    }
    if (HandleList != NULL) {
      FreePool(HandleList);
    }

    //
    // Now check the parent controller using this as the child.
    //
    if (DeviceNameToReturn == NULL) {
      PARSE_HANDLE_DATABASE_PARENTS(
        DeviceHandle,
        &ParentControllerCount,
        &ParentControllerBuffer
        );
      for (LoopVar = 0; LoopVar < ParentControllerCount; LoopVar++) {
        PARSE_HANDLE_DATABASE_UEFI_DRIVERS (
          ParentControllerBuffer[LoopVar],
          &ParentDriverCount,
          &ParentDriverBuffer
          );
        for (HandleCount = 0; HandleCount < ParentDriverCount; HandleCount++) {
          //
          // try using that driver's component name with controller and our
          // driver as the child.
          //
          Status = gBS->OpenProtocol (
                          ParentDriverBuffer[HandleCount],
                          &gEfiComponentName2ProtocolGuid,
                          (VOID**)&CompName2,
                          gImageHandle,
                          NULL,
                          EFI_OPEN_PROTOCOL_GET_PROTOCOL
                          );
          if (EFI_ERROR (Status)) {
            Status = gBS->OpenProtocol (
                            ParentDriverBuffer[HandleCount],
                            &gEfiComponentNameProtocolGuid,
                            (VOID**)&CompName2,
                            gImageHandle,
                            NULL,
                            EFI_OPEN_PROTOCOL_GET_PROTOCOL
                            );
          }

          if (EFI_ERROR (Status)) {
            continue;
          }
          if (Language == NULL) {
            Lang = AllocateZeroPool (
              AsciiStrSize (CompName2->SupportedLanguages)
               );
            if (Lang == NULL) {
              return (EFI_OUT_OF_RESOURCES);
            }
            AsciiStrCpyS (Lang, AsciiStrSize (CompName2->SupportedLanguages), CompName2->SupportedLanguages);
            TempChar = AsciiStrStr (Lang, ";");
            if (TempChar != NULL) {
              *TempChar = CHAR_NULL;
            }
          } else {
            Lang = AllocateZeroPool (AsciiStrSize (Language));
            if (Lang == NULL) {
              return (EFI_OUT_OF_RESOURCES);
            }
            AsciiStrCpyS (Lang, AsciiStrSize (Language), Language);
          }
          Status = CompName2->GetControllerName (
                                CompName2,
                                ParentControllerBuffer[LoopVar],
                                DeviceHandle,
                                Lang,
                                &DeviceNameToReturn
                                );
          FreePool (Lang);
          Lang = NULL;
          if (!EFI_ERROR (Status) && DeviceNameToReturn != NULL) {
            break;
          }
        }
        SHELL_FREE_NON_NULL (ParentDriverBuffer);
        if (!EFI_ERROR (Status) && DeviceNameToReturn != NULL) {
          break;
        }
      }
      SHELL_FREE_NON_NULL (ParentControllerBuffer);
    }
    //
    // dont return on fail since we will try device path if that bit is on
    //
    if (DeviceNameToReturn != NULL) {
      ASSERT (BestDeviceName != NULL);
      StrnCatGrow (BestDeviceName, NULL, DeviceNameToReturn, 0);
      return (EFI_SUCCESS);
    }
  }
  if ((Flags & EFI_DEVICE_NAME_USE_DEVICE_PATH) != 0) {
    Status = gBS->LocateProtocol (
                    &gEfiDevicePathToTextProtocolGuid,
                    NULL,
                    (VOID**)&DevicePathToText
                    );
    //
    // we now have the device path to text protocol
    //
    if (!EFI_ERROR(Status)) {
      Status = gBS->OpenProtocol (
                      DeviceHandle,
                      &gEfiDevicePathProtocolGuid,
                      (VOID**)&DevicePath,
                      gImageHandle,
                      NULL,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );
      if (!EFI_ERROR(Status)) {
        //
        // use device path to text on the device path
        //
        *BestDeviceName = DevicePathToText->ConvertDevicePathToText (
                                              DevicePath,
                                              TRUE,
                                              TRUE
                                              );
        return (EFI_SUCCESS);
      }
    }
  }
  //
  // none of the selected bits worked.
  //
  return (EFI_NOT_FOUND);
}

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
  IN EFI_SERIAL_IO_PROTOCOL    *SerialDevice
  )
{
  Print (L"The current attributes are:\n");
  Print (L"Baudrate: %d \n",SerialDevice->Mode->BaudRate);
  Print (L"Data Bits: %d \n",SerialDevice->Mode->DataBits);
  if (SerialDevice->Mode->Parity == DefaultParity) {
    Print (L"Parity: Default\n");
  }
  if (SerialDevice->Mode->Parity == NoParity) {
    Print (L"Parity: No\n");
  }
  if (SerialDevice->Mode->Parity == EvenParity) {
    Print (L"Parity: Even\n");
  }
  if (SerialDevice->Mode->Parity == OddParity) {
    Print (L"Parity: Odd\n");
  }
  if (SerialDevice->Mode->Parity == MarkParity) {
    Print (L"Parity: Mark\n");
  }
  if (SerialDevice->Mode->Parity == SpaceParity) {
    Print (L"Parity: Space\n");
  }
  if (SerialDevice->Mode->StopBits == DefaultStopBits) {
    Print (L"Stop Bits: Default\n");
  }
  if (SerialDevice->Mode->StopBits == OneStopBit) {
    Print (L"Stop Bits: One\n");
  }
  if (SerialDevice->Mode->StopBits == OneFiveStopBits) {
    Print (L"Stop Bits: One-Five \n");
  }
  if (SerialDevice->Mode->StopBits == TwoStopBits) {
    Print (L"Stop Bits: Two\n");
  }
}


/**
  Displays the current enable/disable state of the control bits.

  @param  Control[in]      The current control value.
**/
VOID
EFIAPI
DisplaySerialDeviceControlBits (
  IN UINT32        Control
  )
{
  Print (L"CURRENT ENABLE/DISABLE STATUS OF CONTROL BITS:\n");
  if ((Control & EFI_SERIAL_CLEAR_TO_SEND) == EFI_SERIAL_CLEAR_TO_SEND) {
    Print (L"Serial Clear To Send: ENABLED\n");
  } else {
    Print (L"Serial Clear TO Send: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_DATA_SET_READY) == EFI_SERIAL_DATA_SET_READY) {
    Print (L"Serial Data Set Ready: ENABLED\n");
  } else {
    Print (L"Serial Data Set Ready: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_RING_INDICATE) == EFI_SERIAL_RING_INDICATE) {
    Print (L"Serial Ring Indicate: ENABLED\n");
  } else {
    Print (L"Serial Ring Indicate: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_CARRIER_DETECT) == EFI_SERIAL_CARRIER_DETECT) {
    Print(L"Serial Carrier Detect: ENABLED\n");
  } else {
    Print(L"Serial Carrier Detect: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_REQUEST_TO_SEND) == EFI_SERIAL_REQUEST_TO_SEND) {
    Print (L"Serial Request To Send: ENABLED\n");
  } else {
    Print (L"Serial Request To Send: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_DATA_TERMINAL_READY) ==
    EFI_SERIAL_DATA_TERMINAL_READY) {
    Print (L"Serial Data Terminal Ready: ENABLED\n");
  } else {
    Print (L"Serial Data Terminal Ready: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_INPUT_BUFFER_EMPTY) ==
    EFI_SERIAL_INPUT_BUFFER_EMPTY) {
    Print (L"Serial Input Buffer Empty: ENABLED\n");
  } else {
    Print (L"Serial Input Buffer Empty: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_OUTPUT_BUFFER_EMPTY) ==
    EFI_SERIAL_OUTPUT_BUFFER_EMPTY) {
    Print (L"Serial Output Buffer Empty: ENABLED\n");
  } else {
    Print (L"Serial Output Buffer Empty: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE) ==
    EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE) {
    Print (L"Hardware Loopback: ENABLED\n");
  } else {
    Print (L"Hardware Loopback: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE) ==
    EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE) {
    Print (L"Software Loopback: ENABLED\n");
  } else {
    Print (L"Software Loopback: DISABLED\n");
  }
  if ((Control & EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE) ==
    EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE) {
    Print (L"Hardware Flow Control: ENABLED\n");
  } else {
    Print (L"Hardware Flow Control: DISABLED\n");
  }
}


/**
  Displays the main menu of the test application.

**/
VOID
EFIAPI
DisplayMainTestMenu (
  VOID
  )
{
  Print (L"Please select from the following options or press ESC to exit...\n");
  Print (L"1. View the current device attributes\n");
  Print (L"2. View the current ENABLE/DISABLE status of the control bits\n");
  Print (L"3. Write to the serial device \n");
  Print (L"4. Set serial device attributes\n");
  Print (L"5. Set ENABLE/DISABLE status of the control bits\n");
}

/**
  Displays the selection menu for parity options.
**/
VOID
EFIAPI
DisplayParityMenu (
  VOID
  )
{
  Print (L"Please select from the following options for the parity setting\n");
  Print (L"1. Default\n");
  Print (L"2. None\n");
  Print (L"3. Even\n");
  Print (L"4. Odd\n");
  Print (L"5. Mark\n");
  Print (L"6. Space\n");
}

/**
  Displays the selection menu for stop bit options.

**/
VOID
EFIAPI
DisplayStopBitsMenu (
  VOID
  )
{
  Print (L"Please select the stop bits setting\n");
  Print (L"1. Default\n");
  Print (L"2. One (1)\n");
  Print (L"3. One-Five (1.5)\n");
  Print (L"4. Two (2)\n");
}

/**
  Displays the selection menu for baudrate options.

**/
VOID
EFIAPI
DisplayBaudRateMenu (
  VOID
  )
{
  Print (L"Please select the baudrate setting\n");
  Print (L"0.    9600\n");
  Print (L"1.   19200\n");
  Print (L"2.   57600\n");
  Print (L"3.  115200\n");
  Print (L"4.  230400\n");
  Print (L"5.  460800\n");
  Print (L"6.  921600\n");
  Print (L"7. 1228800\n");
  Print (L"8. 3000000\n");
  Print (L"9. 6000000\n");
}

/**
  Displays the selection menu for data options.

**/
VOID
EFIAPI
DisplayDataBitsMenu (
  VOID
  )
{
  Print (L"Please select the data bits setting\n");
  Print (L"1. Default\n");
  Print (L"2. Five (5)\n");
  Print (L"3. Six (6)\n");
  Print (L"4. Seven (7)\n");
  Print (L"5. Eight (8)\n");
}

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
  IN UINT64                BaudRate,
  IN EFI_PARITY_TYPE       Parity,
  IN UINT8                 DataBits,
  IN EFI_STOP_BITS_TYPE    StopBits
  )
{
  Print (L"The following attribute values will be set on the device:\n");
  Print (L"Baudrate: %d\n",BaudRate);
  Print (L"Parity: ");
  if (Parity == DefaultParity) {
    Print (L"Default\n");
  }
  if (Parity == NoParity) {
    Print (L"None\n");
  }
  if (Parity == EvenParity) {
    Print (L"Even\n");
  }
  if (Parity == OddParity) {
    Print (L"Odd\n");
  }
  if (Parity == MarkParity) {
    Print (L"Mark\n");
  }
  if (Parity == SpaceParity) {
    Print (L"Space\n");
  }
  Print (L"Data Bits: %d\n",DataBits);
  Print (L"Stop Bits: ");
  if (StopBits == DefaultStopBits) {
    Print(L"Default\n");
  }
  if (StopBits == OneStopBit) {
    Print(L"1\n");
  }
  if (StopBits == OneFiveStopBits) {
    Print(L"1.5\n");
  }
  if (StopBits == TwoStopBits) {
    Print(L"2\n");
  }
}

/**
  Displays the selection menu for setting the Usb Serial Device Attributes.

**/
VOID 
EFIAPI
DisplaySetAttributesMenu (
  VOID
  )
{
  Print (L"Please select which attribute to set \n");
  Print (L"Press ESC to set the attributes and return to the main menu\n");
  Print (L"1. Baudrate\n");
  Print (L"2. Parity\n");
  Print (L"3. Data Bits\n");
  Print (L"4. Stop Bits\n");
}

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
  IN EFI_SERIAL_IO_PROTOCOL    *SerialIo
  )
{
  EFI_STATUS            Status;
  EFI_INPUT_KEY         Keystroke;
  UINTN                 Index;

  //
  // the attributes which will be set
  //
  UINT64                BaudRate;
  UINT32                ReceiveFifoDepth;
  UINT32                Timeout;
  EFI_PARITY_TYPE       Parity;
  UINT8                DataBits;
  EFI_STOP_BITS_TYPE    StopBits;

  Status = EFI_UNSUPPORTED;
  Keystroke.UnicodeChar = 0x0;
  Keystroke.ScanCode    = 0x0;

  Print (L"At this time Timeout and Receive Fifo Depth cannot be changed\n");

  //
  // set the components of Attributes to their current values since SerialIo
  // SetAttributes requires all values passed to it
  //
  BaudRate         = SerialIo->Mode->BaudRate;
  Parity           = SerialIo->Mode->Parity;
  DataBits         = (UINT8) SerialIo->Mode->DataBits;
  StopBits         = SerialIo->Mode->StopBits;
  ReceiveFifoDepth = 1024;
  Timeout          = 16;

  //
  // Display the SetAttributes menu to the user and get the users selection.
  // Depending on the users selection display the appropriate submenu and retrieve
  // the value to be set for that attribute. Also sets the selected attributes on
  // the device.
  //
  while (Keystroke.ScanCode != 0x17) {
    while (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x34) {
      DisplaySetAttributesMenu ();
      Status = EFI_INVALID_PARAMETER;
      while (EFI_ERROR(Status)) {
        Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
      }
      gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);

      if (Keystroke.ScanCode == 0x17) {
        break;
      }
      if (Keystroke.UnicodeChar == 0x31) {
        //
        // The user has selected the baudrate option.
        //
        Keystroke.UnicodeChar = 0x0;
        Keystroke.ScanCode    = 0x0;
        DisplayBaudRateMenu ();
        while (Keystroke.UnicodeChar < 0x30 || Keystroke.UnicodeChar > 0x39) {
          //
          // Get the users selection for baudrate value.
          //
          Status = EFI_INVALID_PARAMETER;
          while (EFI_ERROR (Status)) {
            Status = gBS->WaitForEvent (1,(&gST->ConIn->WaitForKey), &Index);
          }
          gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
          if (Keystroke.ScanCode == 0x17) {
            break;
          }
          if (Keystroke.UnicodeChar == 0x30) {
            BaudRate = 9600;
          }
          if (Keystroke.UnicodeChar == 0x31) {
            BaudRate = 19200;
          }
          if (Keystroke.UnicodeChar == 0x32) {
            BaudRate = 57600;
          }
          if (Keystroke.UnicodeChar == 0x33) {
            BaudRate = 115200;
          }
          if (Keystroke.UnicodeChar == 0x34) {
            BaudRate = 230400;
          }
          if (Keystroke.UnicodeChar == 0x35) {
            BaudRate = 460800;
          }
          if (Keystroke.UnicodeChar == 0x36) {
            BaudRate = 921600;
          }
          if (Keystroke.UnicodeChar == 0x37) {
            BaudRate = 1228800;
          }
          if (Keystroke.UnicodeChar == 0x38) {
            BaudRate = 3000000;
          }
          if (Keystroke.UnicodeChar == 0x39) {
            BaudRate = 6000000;
          }
        }
        Keystroke.ScanCode    = 0x0;
        Keystroke.UnicodeChar = 0x0;
      }
      if (Keystroke.UnicodeChar == 0x32) {
        //
        // The user has selected the set parity option.
        Keystroke.UnicodeChar = 0x0;
        Keystroke.ScanCode    = 0x0;
        DisplayParityMenu ();
        while (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x36) {
          //
          // Get the users selection for parity value.
          //
          Status = EFI_INVALID_PARAMETER;
          while (EFI_ERROR (Status)) {
            Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
          }
          gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
          if (Keystroke.ScanCode == 0x17) {
            break;
          }
          if (Keystroke.UnicodeChar == 0x31) {
            Parity = DefaultParity;
          }
          if (Keystroke.UnicodeChar == 0x32) {
            Parity = NoParity;
          }
          if (Keystroke.UnicodeChar == 0x33) {
            Parity = EvenParity;
          }
          if (Keystroke.UnicodeChar == 0x34) {
            Parity = OddParity;
          }
          if (Keystroke.UnicodeChar == 0x35) {
            Parity = MarkParity;
          }
          if (Keystroke.UnicodeChar == 0x36) {
            Parity = SpaceParity;
          }
        }
        Keystroke.ScanCode    = 0x0;
        Keystroke.UnicodeChar = 0x0;
      }
      if (Keystroke.UnicodeChar == 0x33) {
        //
        // The user has selected the set data bits option.
        //
        Keystroke.UnicodeChar = 0x0;
        Keystroke.ScanCode    = 0x0;
        DisplayDataBitsMenu ();
        while (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x35) {
          //
          // Get the users selection for the data bits value.
          //
          Status = EFI_INVALID_PARAMETER;
          while (EFI_ERROR (Status)) {
              Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
          }
          gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
          if (Keystroke.ScanCode == 0x17) {
            break;
          }
          if (Keystroke.UnicodeChar == 0x31) { //this is the option for the default data bits setting which is 8
            DataBits = 8;
          }
          if (Keystroke.UnicodeChar == 0x32) {
            DataBits = 5;
          }
          if (Keystroke.UnicodeChar == 0x33) {
            DataBits = 6;
          }
          if (Keystroke.UnicodeChar == 0x34) {
            DataBits = 7;
          }
          if (Keystroke.UnicodeChar == 0x35) {
            DataBits = 8;
          }
        }
        Keystroke.ScanCode    = 0x0;
        Keystroke.UnicodeChar = 0x0;
      }

      if (Keystroke.UnicodeChar == 0x34) {
        //
        // The user has selected the set stop bits option.
        //
        Keystroke.UnicodeChar = 0x0;
        Keystroke.ScanCode    = 0x0;
        DisplayStopBitsMenu ();
        while (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x34) {
          //
          // Get the users selection for the stop bits value.
          Status = EFI_INVALID_PARAMETER;
          while (EFI_ERROR (Status)) {
            Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
          }
          gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
          if (Keystroke.ScanCode == 0x17) {
            break;
          }
          if (Keystroke.UnicodeChar == 0x31) {
            StopBits = DefaultStopBits;
          }
          if (Keystroke.UnicodeChar == 0x32) {
            StopBits = OneStopBit;
          }
          if (Keystroke.UnicodeChar == 0x33) {
            StopBits = OneFiveStopBits;
          }
          if (Keystroke.UnicodeChar == 0x34) {
            StopBits = TwoStopBits;
          }
        }
        Keystroke.ScanCode    = 0x0;
        Keystroke.UnicodeChar = 0x0;
      }
      //
      // Display the attributes that the user has selected and set them on the
      // device.
      //
      DisplayAttributesToBeSet (BaudRate, Parity, DataBits, StopBits);
      Status = SerialIo->SetAttributes (
                           SerialIo,
                           BaudRate,
                           ReceiveFifoDepth,
                           Timeout,
                           Parity,
                           DataBits,
                           StopBits
                           );
    }
    Keystroke.UnicodeChar = 0x0;
  }
  return Status;    
}

/**
  Displays the enable disable menu that is used when the user is setting the
  control bits.

**/
VOID
EFIAPI
DisplayEnableDisableMenu (
  VOID
  )
{
  Print (L"Please select from the following options:\n");
  Print (L"1. Enable\n");
  Print (L"2. Disable\n");
}

/**
  Displays which control bits can be set and prompts the user for a selection.

**/
VOID
EFIAPI
DisplayControlBitMenu (
  VOID
  )
{
  Print (L"Select which control bit you would like to set:\n");
  Print (L"Press ESC to return to main menu\n");
  Print (L"At this time Hardware and Software Loopback cannot be enabled\n");
  Print (L"1. REQUEST_TO_SEND\n");
  Print (L"2. DATA_TERMINAL_READY\n");
  Print (L"3. HARDWARE_FLOW_CONTROL\n");
}

/**
  Sets the control bits on the device based on user input.

  The user is prompted with the control bits which can be set. Once the user has
  selected the control bits and the values they wish to set on them they are
  placed in the format expected by the Serial IO protocol SetControl method. The
  control bits are then set on the device using SetConrol.

  @param  SerialIo[in]      The current instance of the Serial Io Protocol.

  @retval EFI_SUCCESS       The user was able to exit the test successfully.

**/
EFI_STATUS 
EFIAPI
SetControlBitsTest (
  IN EFI_SERIAL_IO_PROTOCOL    *SerialIo
  )
{
  EFI_STATUS        Status;
  EFI_INPUT_KEY     Keystroke;
  UINTN             Index;
  BOOLEAN           RequestToSend;
  BOOLEAN           DataTerminalReady;
  BOOLEAN           HardwareFlowControl;
  UINT32            *Control;

  Status                = EFI_UNSUPPORTED;
  Keystroke.UnicodeChar = 0x0;
  Keystroke.ScanCode    = 0x0;
  RequestToSend         = FALSE;
  DataTerminalReady     = FALSE;
  HardwareFlowControl   = FALSE;
  Control = 0;

  //
  // Display the set control menu and depending on the users response display the
  // appropriate submenu and set the proper value for the selected control bit.
  //
  while (Keystroke.ScanCode != 0x17) {
    while (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x33) {
      DisplayControlBitMenu ();
      Status = EFI_INVALID_PARAMETER;
      while (EFI_ERROR (Status)) {
        Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
      }
      Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
      if (Keystroke.ScanCode == 0x17) {
        break;
      }
      if (Keystroke.UnicodeChar == 0x31) {
        //
        // The user has selected the set Request to Send option.
        //
        Keystroke.ScanCode    = 0x0;
        Keystroke.UnicodeChar = 0x0;
        DisplayEnableDisableMenu ();
        while (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x32) {
          Status = EFI_INVALID_PARAMETER;
          while (EFI_ERROR(Status)) {
            //
            // Get the users response for the enable/disable value of the control
            // bit
            //
            Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
          }
          Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
          if (Keystroke.ScanCode == 0x17) {
            break;
          }
          if (Keystroke.UnicodeChar == 0x31) {
            RequestToSend = TRUE;
          }
          if (Keystroke.UnicodeChar == 0x32) {
            RequestToSend = FALSE;
          }
        }
        Keystroke.UnicodeChar = 0x0;
        Keystroke.ScanCode    = 0x0;
      }
      if (Keystroke.UnicodeChar == 0x32) {
        //
        // The user has selected the set Data Terminal Ready option.
        //
        Keystroke.ScanCode    = 0x0;
        Keystroke.UnicodeChar = 0x0;
        DisplayEnableDisableMenu ();
        while (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x32) {
          //
          // Get the users response for the enable/disable value of the control
          // bit.
          //
          Status = EFI_INVALID_PARAMETER;
          while (EFI_ERROR(Status)) {
            Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
          }
          Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
          if (Keystroke.UnicodeChar == 0x31) {
            DataTerminalReady = TRUE;
          }
          if (Keystroke.UnicodeChar == 0x32) {
            DataTerminalReady = FALSE;
          }
        }
        Keystroke.UnicodeChar = 0x0;
        Keystroke.ScanCode    = 0x0;
      }
      if (Keystroke.UnicodeChar == 0x33) {
        //
        // The user has selected the set hardware flow control option.
        //
        Keystroke.ScanCode    = 0x0;
        Keystroke.UnicodeChar = 0x0;
        DisplayEnableDisableMenu();
        while (Keystroke.UnicodeChar < 0x31 || Keystroke.UnicodeChar > 0x32) {
          //
          // Get the users response for the enable/disable value of the control
          // bit.
          //
          Status = EFI_INVALID_PARAMETER;
          while (EFI_ERROR (Status)) {
            Status = gBS->WaitForEvent (1, &(gST->ConIn->WaitForKey), &Index);
          }
          Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
          if (Keystroke.UnicodeChar == 0x31) {
            HardwareFlowControl = TRUE;
          }
          if (Keystroke.UnicodeChar == 0x32) {
            HardwareFlowControl = FALSE;
          }
        }
        Keystroke.UnicodeChar = 0x0;
        Keystroke.ScanCode    = 0x0;
      }
    }
    //
    // adjust the control value according to the users input
    //
    if (RequestToSend) {
      *Control |= EFI_SERIAL_REQUEST_TO_SEND;
    } else if (Control != 0) {
      *Control &= EFI_SERIAL_REQUEST_TO_SEND;
    }
    if (DataTerminalReady) {
      *Control |= EFI_SERIAL_DATA_TERMINAL_READY;
    } else if (Control != 0) {
      *Control &= EFI_SERIAL_DATA_TERMINAL_READY;
    }
    if (HardwareFlowControl) {
      *Control |= EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE;
    } else if (Control !=0) {
      *Control &= EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE;
    }

    SerialIo->SetControl (SerialIo, *Control);
    Keystroke.UnicodeChar = 0x0;
  }
  return Status;
}
