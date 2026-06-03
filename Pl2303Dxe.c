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
 * Per-type capability table
 * ========================================================================= */

STATIC CONST PL2303_TYPE_DATA Pl2303TypeData[TYPE_COUNT] = {
  /* TYPE_H   */ { "H",   1228800, PL2303_QUIRK_LEGACY, TRUE,  FALSE, FALSE },
  /* TYPE_HX  */ { "HX",  6000000, 0,                   FALSE, FALSE, FALSE },
  /* TYPE_TA  */ { "TA",  6000000, 0,                   FALSE, FALSE, TRUE  },
  /* TYPE_TB  */ { "TB", 12000000, 0,                   FALSE, FALSE, TRUE  },
  /* TYPE_HXD */ { "HXD",12000000, 0,                   FALSE, FALSE, FALSE },
  /* TYPE_HXN */ { "HXN", 12000000, 0,                   FALSE, TRUE,  FALSE },
};

/* =========================================================================
 * Supported-device table  (VID / PID / quirks)
 * ========================================================================= */

STATIC CONST PL2303_USB_DEVICE mPl2303DeviceList[] = {
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID,            PL2303_QUIRK_ENDPOINT_HACK },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_RSAQ2,      0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_DCU11,      0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_RSAQ3,      0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_CHILITAG,   0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_PHAROS,     0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_ALDIGA,     0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_MMX,        0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_GPRS,       0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_HCR331,     0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_MOTOROLA,   0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_ZTEK,       0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_TB,         0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_GC,         0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_GB,         0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_GT,         0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_GL,         0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_GE,         0 },
  { PL2303_VENDOR_ID,      PL2303_PRODUCT_ID_GS,         0 },
  { IODATA_VENDOR_ID,      IODATA_PRODUCT_ID,            0 },
  { IODATA_VENDOR_ID,      IODATA_PRODUCT_ID_RSAQ5,      0 },
  { ATEN_VENDOR_ID,        ATEN_PRODUCT_ID,              PL2303_QUIRK_ENDPOINT_HACK },
  { ATEN_VENDOR_ID,        ATEN_PRODUCT_UC485,           PL2303_QUIRK_ENDPOINT_HACK },
  { ATEN_VENDOR_ID,        ATEN_PRODUCT_UC232B,          PL2303_QUIRK_ENDPOINT_HACK },
  { ATEN_VENDOR_ID,        ATEN_PRODUCT_ID2,             0 },
  { ATEN_VENDOR_ID2,       ATEN_PRODUCT_ID,              0 },
  { ELCOM_VENDOR_ID,       ELCOM_PRODUCT_ID,             0 },
  { ELCOM_VENDOR_ID,       ELCOM_PRODUCT_ID_UCSGT,       0 },
  { ITEGNO_VENDOR_ID,      ITEGNO_PRODUCT_ID,            0 },
  { ITEGNO_VENDOR_ID,      ITEGNO_PRODUCT_ID_2080,       0 },
  { MA620_VENDOR_ID,       MA620_PRODUCT_ID,             0 },
  { RATOC_VENDOR_ID,       RATOC_PRODUCT_ID,             0 },
  { TRIPP_VENDOR_ID,       TRIPP_PRODUCT_ID,             0 },
  { RADIOSHACK_VENDOR_ID,  RADIOSHACK_PRODUCT_ID,        0 },
  { DCU10_VENDOR_ID,       DCU10_PRODUCT_ID,             0 },
  { SITECOM_VENDOR_ID,     SITECOM_PRODUCT_ID,           0 },
  { ALCATEL_VENDOR_ID,     ALCATEL_PRODUCT_ID,           0 },
  { SIEMENS_VENDOR_ID,     SIEMENS_PRODUCT_ID_SX1,       PL2303_QUIRK_UART_STATE_IDX0 },
  { SIEMENS_VENDOR_ID,     SIEMENS_PRODUCT_ID_X65,       PL2303_QUIRK_UART_STATE_IDX0 },
  { SIEMENS_VENDOR_ID,     SIEMENS_PRODUCT_ID_X75,       PL2303_QUIRK_UART_STATE_IDX0 },
  { SIEMENS_VENDOR_ID,     SIEMENS_PRODUCT_ID_EF81,      PL2303_QUIRK_ENDPOINT_HACK   },
  { BENQ_VENDOR_ID,        BENQ_PRODUCT_ID_S81,          0 },
  { SYNTECH_VENDOR_ID,     SYNTECH_PRODUCT_ID,           0 },
  { NOKIA_CA42_VENDOR_ID,  NOKIA_CA42_PRODUCT_ID,        0 },
  { CA_42_CA42_VENDOR_ID,  CA_42_CA42_PRODUCT_ID,        0 },
  { SAGEM_VENDOR_ID,       SAGEM_PRODUCT_ID,             0 },
  { LEADTEK_VENDOR_ID,     LEADTEK_9531_PRODUCT_ID,      0 },
  { SPEEDDRAGON_VENDOR_ID, SPEEDDRAGON_PRODUCT_ID,       0 },
  { DATAPILOT_U2_VENDOR_ID,DATAPILOT_U2_PRODUCT_ID,      0 },
  { BELKIN_VENDOR_ID,      BELKIN_PRODUCT_ID,            0 },
  { ALCOR_VENDOR_ID,       ALCOR_PRODUCT_ID,             PL2303_QUIRK_ENDPOINT_HACK   },
  { WS002IN_VENDOR_ID,     WS002IN_PRODUCT_ID,           0 },
  { COREGA_VENDOR_ID,      COREGA_PRODUCT_ID,            0 },
  { YCCABLE_VENDOR_ID,     YCCABLE_PRODUCT_ID,           0 },
  { SUPERIAL_VENDOR_ID,    SUPERIAL_PRODUCT_ID,          0 },
  { HP_VENDOR_ID,          HP_LD220_PRODUCT_ID,          0 },
  { HP_VENDOR_ID,          HP_LD220TA_PRODUCT_ID,        0 },
  { HP_VENDOR_ID,          HP_LD381_PRODUCT_ID,          0 },
  { HP_VENDOR_ID,          HP_LD381GC_PRODUCT_ID,        0 },
  { HP_VENDOR_ID,          HP_LD960_PRODUCT_ID,          0 },
  { HP_VENDOR_ID,          HP_LD960TA_PRODUCT_ID,        0 },
  { HP_VENDOR_ID,          HP_LCM220_PRODUCT_ID,         0 },
  { HP_VENDOR_ID,          HP_LCM960_PRODUCT_ID,         0 },
  { HP_VENDOR_ID,          HP_LM920_PRODUCT_ID,          0 },
  { HP_VENDOR_ID,          HP_LM930_PRODUCT_ID,          0 },
  { HP_VENDOR_ID,          HP_LM940_PRODUCT_ID,          0 },
  { HP_VENDOR_ID,          HP_TD620_PRODUCT_ID,          0 },
  { CRESSI_VENDOR_ID,      CRESSI_EDY_PRODUCT_ID,        0 },
  { ZEAGLE_VENDOR_ID,      ZEAGLE_N2ITION3_PRODUCT_ID,   0 },
  { SONY_VENDOR_ID,        SONY_QN3USB_PRODUCT_ID,       0 },
  { SANWA_VENDOR_ID,       SANWA_PRODUCT_ID,             0 },
  { ADLINK_VENDOR_ID,      ADLINK_ND6530_PRODUCT_ID,     0 },
  { ADLINK_VENDOR_ID,      ADLINK_ND6530GC_PRODUCT_ID,   0 },
  { SMART_VENDOR_ID,       SMART_PRODUCT_ID,             0 },
  { AT_VENDOR_ID,          AT_VTKIT3_PRODUCT_ID,         0 },
  { IBM_VENDOR_ID,         IBM_PRODUCT_ID,               0 },
  { MACROSILICON_VENDOR_ID,MACROSILICON_MS3020_PRODUCT_ID,0 },
  { 0, 0, 0 }  /* terminator */
};

/* =========================================================================
 * Global driver-binding instance
 * ========================================================================= */

EFI_DRIVER_BINDING_PROTOCOL gPl2303DriverBinding = {
  Pl2303DriverBindingSupported,
  Pl2303DriverBindingStart,
  Pl2303DriverBindingStop,
  0x10,
  NULL,
  NULL
};

/* =========================================================================
 * Internal helpers – device-table lookup
 * ========================================================================= */

/**
  Check whether a USB device (identified through UsbIo) appears in the
  supported-device table.

  @param[in]  UsbIo   USB I/O protocol for the device to test.
  @param[out] Quirks  Receives the quirk flags when the device is found.

  @retval TRUE   Device is in the table; *Quirks is valid.
  @retval FALSE  Device is not supported.
**/
STATIC
BOOLEAN
Pl2303IsDeviceSupported (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  OUT UINTN                *Quirks
  )
{
  EFI_USB_DEVICE_DESCRIPTOR  DevDesc;
  EFI_STATUS                 Status;
  UINTN                      i;

  Status = UsbIo->UsbGetDeviceDescriptor (UsbIo, &DevDesc);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  for (i = 0; mPl2303DeviceList[i].VendorId != 0; i++) {
    if (mPl2303DeviceList[i].VendorId  == DevDesc.IdVendor  &&
        mPl2303DeviceList[i].ProductId == DevDesc.IdProduct) {
      *Quirks = mPl2303DeviceList[i].Quirks;
      return TRUE;
    }
  }
  return FALSE;
}

/* =========================================================================
 * USB control-transfer helpers
 * ========================================================================= */

/**
  Issue a vendor-class IN control transfer to read one byte from a register.

  @param[in]  UsbIo   USB I/O protocol instance.
  @param[in]  IsHxn   TRUE if the chip is TYPE_HXN (uses alternate request code).
  @param[in]  Value   Register address (wValue field).
  @param[out] Buf     Receives the single byte read from the device.

  @retval EFI_SUCCESS       Transfer succeeded.
  @retval EFI_DEVICE_ERROR  Transfer failed.
**/
STATIC
EFI_STATUS
Pl2303VendorRead (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  BOOLEAN               IsHxn,
  IN  UINT16                Value,
  OUT UINT8                *Buf
  )
{
  EFI_USB_DEVICE_REQUEST  Req;
  UINT32                  UsbStatus;
  EFI_STATUS              Status;

  Req.RequestType = VENDOR_READ_REQUEST_TYPE;
  Req.Request     = IsHxn ? VENDOR_READ_NREQUEST : VENDOR_READ_REQUEST;
  Req.Value       = Value;
  Req.Index       = 0;
  Req.Length      = 1;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Req,
                    EfiUsbDataIn,
                    100,
                    Buf,
                    1,
                    &UsbStatus
                    );
  if (EFI_ERROR (Status) || (UsbStatus != 0)) {
    DEBUG ((DEBUG_ERROR,
            "Pl2303VendorRead [%04x] failed: %r (USB status 0x%x)\n",
            Value, Status, UsbStatus));
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}

/**
  Issue a vendor-class OUT control transfer to write a value to a register.
  No data phase; the data is encoded in the wIndex field.

  @param[in]  UsbIo   USB I/O protocol instance.
  @param[in]  IsHxn   TRUE if the chip is TYPE_HXN.
  @param[in]  Value   Register address (wValue field).
  @param[in]  Index   Data to write (wIndex field).

  @retval EFI_SUCCESS       Transfer succeeded.
  @retval EFI_DEVICE_ERROR  Transfer failed.
**/
STATIC
EFI_STATUS
Pl2303VendorWrite (
  IN EFI_USB_IO_PROTOCOL  *UsbIo,
  IN BOOLEAN               IsHxn,
  IN UINT16                Value,
  IN UINT16                Index
  )
{
  EFI_USB_DEVICE_REQUEST  Req;
  UINT32                  UsbStatus;
  EFI_STATUS              Status;

  Req.RequestType = VENDOR_WRITE_REQUEST_TYPE;
  Req.Request     = IsHxn ? VENDOR_WRITE_NREQUEST : VENDOR_WRITE_REQUEST;
  Req.Value       = Value;
  Req.Index       = Index;
  Req.Length      = 0;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Req,
                    EfiUsbNoData,
                    100,
                    NULL,
                    0,
                    &UsbStatus
                    );
  if (EFI_ERROR (Status) || (UsbStatus != 0)) {
    DEBUG ((DEBUG_ERROR,
            "Pl2303VendorWrite [%04x]=%04x failed: %r (USB status 0x%x)\n",
            Value, Index, Status, UsbStatus));
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}

/**
  Read-modify-write a vendor register (for TYPE_HXN the read uses the plain
  address; for older chips the read uses address | 0x80).

  @param[in]  Dev   Driver private data.
  @param[in]  Reg   Register index.
  @param[in]  Mask  Bitmask of bits to update.
  @param[in]  Val   New bit values (only bits set in Mask are applied).

  @retval EFI_SUCCESS       Operation succeeded.
  @retval EFI_DEVICE_ERROR  A USB transfer failed.
**/
STATIC
EFI_STATUS
Pl2303UpdateReg (
  IN PL2303_PRIVATE_DATA  *Dev,
  IN UINT8                 Reg,
  IN UINT8                 Mask,
  IN UINT8                 Val
  )
{
  EFI_STATUS  Status;
  UINT8       Buf;
  BOOLEAN     IsHxn;
  UINT16      ReadAddr;

  IsHxn    = (BOOLEAN)(Dev->Type == TYPE_HXN);
  ReadAddr = IsHxn ? (UINT16)Reg : (UINT16)(Reg | 0x80);

  Status = Pl2303VendorRead (Dev->UsbIo, IsHxn, ReadAddr, &Buf);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Buf = (UINT8)((Buf & ~Mask) | (Val & Mask));
  return Pl2303VendorWrite (Dev->UsbIo, IsHxn, Reg, Buf);
}

/* =========================================================================
 * Chip-type detection helpers
 * ========================================================================= */

/**
  Check whether the device responds to the HX status query (used to
  distinguish TYPE_TA/TB from the generic HXN).

  @param[in]  UsbIo  USB I/O protocol instance.

  @retval TRUE  The device supports the HX status query.
  @retval FALSE It does not.
**/
STATIC
BOOLEAN
Pl2303SupportsHxStatus (
  IN EFI_USB_IO_PROTOCOL  *UsbIo
  )
{
  EFI_USB_DEVICE_REQUEST  Req;
  UINT32                  UsbStatus;
  EFI_STATUS              Status;
  UINT8                   Buf;

  Req.RequestType = VENDOR_READ_REQUEST_TYPE;
  Req.Request     = VENDOR_READ_REQUEST;
  Req.Value       = PL2303_READ_TYPE_HX_STATUS;
  Req.Index       = 0;
  Req.Length      = 1;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Req,
                    EfiUsbDataIn,
                    100,
                    &Buf,
                    1,
                    &UsbStatus
                    );
  return (!EFI_ERROR (Status) && (UsbStatus == 0));
}

/**
  Determine the PL2303 chip variant from the USB device descriptor.

  Mirrors pl2303_detect_type() from the Linux driver.

  @param[in]  UsbIo  USB I/O protocol instance.

  @return  A PL2303_TYPE value, or -1 on unrecognised device.
**/
STATIC
INT32
Pl2303DetectType (
  IN EFI_USB_IO_PROTOCOL  *UsbIo
  )
{
  EFI_USB_DEVICE_DESCRIPTOR  Desc;
  EFI_STATUS                 Status;
  UINT16                     BcdDevice;
  UINT16                     BcdUsb;

  Status = UsbIo->UsbGetDeviceDescriptor (UsbIo, &Desc);
  if (EFI_ERROR (Status)) {
    return -1;
  }

  if (Desc.DeviceClass == 0x02) {
    return TYPE_H;    /* Legacy H, variant 0 */
  }

  if (Desc.MaxPacketSize0 != 0x40) {
    /* Legacy H, variant 0 or 1 */
    return TYPE_H;
  }

  BcdDevice = Desc.BcdDevice;
  BcdUsb    = Desc.BcdUSB;

  switch (BcdUsb) {
  case 0x101:
    /* Treat USB 1.0.1 as 1.1 */
    /* fall through */
  case 0x110:
    switch (BcdDevice) {
    case 0x400:
      return TYPE_HXD;
    case 0x300:
    default:
      return TYPE_HX;
    }

  case 0x200:
    switch (BcdDevice) {
    case 0x100:
    case 0x105:
      return TYPE_HXN;
    case 0x300:    /* GT / TA */
      if (Pl2303SupportsHxStatus (UsbIo)) {
        return TYPE_TA;
      }
      /* fall through */
    case 0x305:
    case 0x400:    /* GL */
    case 0x405:
      return TYPE_HXN;
    case 0x500:    /* GE / TB */
      if (Pl2303SupportsHxStatus (UsbIo)) {
        return TYPE_TB;
      }
      /* fall through */
    case 0x505:
    case 0x600:    /* GS */
    case 0x605:
    case 0x700:    /* GR */
    case 0x705:
    case 0x905:    /* GT-2AB */
    case 0x1005:   /* GC-Q20 */
      return TYPE_HXN;
    default:
      break;
    }
    break;

  default:
    break;
  }

  DEBUG ((DEBUG_WARN, "PL2303: unknown device type (bcdUSB=%04x bcdDevice=%04x)\n",
          BcdUsb, BcdDevice));
  return -1;
}

/**
  Detect HXD clone chips that stall the GET_LINE_REQUEST.

  @param[in]  UsbIo  USB I/O protocol instance.

  @retval TRUE  The device stalled (clone behaviour).
  @retval FALSE Normal HXD response.
**/
STATIC
BOOLEAN
Pl2303IsHxdClone (
  IN EFI_USB_IO_PROTOCOL  *UsbIo
  )
{
  EFI_USB_DEVICE_REQUEST  Req;
  UINT32                  UsbStatus;
  EFI_STATUS              Status;
  UINT8                   Buf[7];

  Req.RequestType = GET_LINE_REQUEST_TYPE;
  Req.Request     = GET_LINE_REQUEST;
  Req.Value       = 0;
  Req.Index       = 0;
  Req.Length      = 7;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Req,
                    EfiUsbDataIn,
                    100,
                    Buf,
                    7,
                    &UsbStatus
                    );
  /* A clone returns a STALL / error instead of valid data. */
  return EFI_ERROR (Status);
}

/* =========================================================================
 * Endpoint discovery
 * ========================================================================= */

/**
  Enumerate the endpoints of the current USB interface and populate the
  bulk-in, bulk-out, and interrupt-in addresses in Dev.

  @param[in,out]  Dev  Driver private data with UsbIo already set.

  @retval EFI_SUCCESS       Both bulk endpoints were found.
  @retval EFI_NOT_FOUND     Required bulk endpoint(s) are missing.
  @retval other             A USB I/O call failed.
**/
STATIC
EFI_STATUS
Pl2303DiscoverEndpoints (
  IN OUT PL2303_PRIVATE_DATA  *Dev
  )
{
  EFI_USB_INTERFACE_DESCRIPTOR  IfDesc;
  EFI_USB_ENDPOINT_DESCRIPTOR   EpDesc;
  EFI_STATUS                    Status;
  UINT8                         i;

  Status = Dev->UsbIo->UsbGetInterfaceDescriptor (Dev->UsbIo, &IfDesc);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (i = 0; i < IfDesc.NumEndpoints; i++) {
    Status = Dev->UsbIo->UsbGetEndpointDescriptor (Dev->UsbIo, i, &EpDesc);
    if (EFI_ERROR (Status)) {
      continue;
    }

    switch (EpDesc.Attributes & USB_ENDPOINT_TYPE_MASK) {
    case USB_ENDPOINT_BULK:
      if ((EpDesc.EndpointAddress & BIT7) != 0) {
        Dev->BulkInAddr  = EpDesc.EndpointAddress;
      } else {
        Dev->BulkOutAddr = EpDesc.EndpointAddress;
      }
      break;

    case USB_ENDPOINT_INTERRUPT:
      if ((EpDesc.EndpointAddress & BIT7) != 0) {
        Dev->IntInAddr = EpDesc.EndpointAddress;
      }
      break;

    default:
      break;
    }
  }

  if ((Dev->BulkInAddr == 0) || (Dev->BulkOutAddr == 0)) {
    DEBUG ((DEBUG_ERROR, "PL2303: bulk endpoints not found\n"));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/* =========================================================================
 * Device initialisation (mirrors pl2303_startup)
 * ========================================================================= */

/**
  Send the initialisation sequence expected by non-HXN PL2303 chips.

  @param[in]  Dev  Driver private data.

  @retval EFI_SUCCESS       Sequence completed without error.
  @retval EFI_DEVICE_ERROR  A USB transfer failed.
**/
STATIC
EFI_STATUS
Pl2303Startup (
  IN PL2303_PRIVATE_DATA  *Dev
  )
{
  EFI_STATUS  Status;
  UINT8       Buf;
  BOOLEAN     IsHxn;
  BOOLEAN     IsLegacy;

  IsHxn    = (BOOLEAN)(Dev->Type == TYPE_HXN);
  IsLegacy = (BOOLEAN)((Dev->Quirks & PL2303_QUIRK_LEGACY) != 0);

  if (IsHxn) {
    /* HXN-family does not require the init sequence. */
    return EFI_SUCCESS;
  }

  Status = Pl2303VendorRead  (Dev->UsbIo, FALSE, 0x8484, &Buf);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorWrite (Dev->UsbIo, FALSE, 0x0404, 0);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorRead  (Dev->UsbIo, FALSE, 0x8484, &Buf);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorRead  (Dev->UsbIo, FALSE, 0x8383, &Buf);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorRead  (Dev->UsbIo, FALSE, 0x8484, &Buf);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorWrite (Dev->UsbIo, FALSE, 0x0404, 1);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorRead  (Dev->UsbIo, FALSE, 0x8484, &Buf);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorRead  (Dev->UsbIo, FALSE, 0x8383, &Buf);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorWrite (Dev->UsbIo, FALSE, 0, 1);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorWrite (Dev->UsbIo, FALSE, 1, 0);
  if (EFI_ERROR (Status)) { return Status; }
  Status = Pl2303VendorWrite (Dev->UsbIo, FALSE, 2, IsLegacy ? 0x24 : 0x44);
  if (EFI_ERROR (Status)) { return Status; }

  return EFI_SUCCESS;
}

/* =========================================================================
 * Modem-control line and line-coding helpers
 * ========================================================================= */

/**
  Apply the current LineControl byte (DTR / RTS) to the device.

  @param[in]  Dev  Driver private data.

  @retval EFI_SUCCESS       Transfer succeeded.
  @retval EFI_DEVICE_ERROR  Transfer failed.
**/
STATIC
EFI_STATUS
Pl2303SetControlLines (
  IN PL2303_PRIVATE_DATA  *Dev
  )
{
  EFI_USB_DEVICE_REQUEST  Req;
  UINT32                  UsbStatus;
  EFI_STATUS              Status;

  DEBUG ((DEBUG_VERBOSE, "PL2303: SetControlLines 0x%02x\n", Dev->LineControl));

  Req.RequestType = SET_CONTROL_REQUEST_TYPE;
  Req.Request     = SET_CONTROL_REQUEST;
  Req.Value       = Dev->LineControl;
  Req.Index       = 0;
  Req.Length      = 0;

  Status = Dev->UsbIo->UsbControlTransfer (
                         Dev->UsbIo,
                         &Req,
                         EfiUsbNoData,
                         100,
                         NULL,
                         0,
                         &UsbStatus
                         );
  if (EFI_ERROR (Status) || (UsbStatus != 0)) {
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}

/**
  Read the 7-byte CDC line-coding from the device.

  If the NO_BREAK_GETLINE quirk is set the last cached value is returned
  instead of querying the hardware.

  @param[in]  Dev  Driver private data.
  @param[out] Buf  Seven-byte buffer to receive the line-coding.

  @retval EFI_SUCCESS       Data is valid.
  @retval EFI_DEVICE_ERROR  USB transfer failed.
**/
STATIC
EFI_STATUS
Pl2303GetLineRequest (
  IN  PL2303_PRIVATE_DATA  *Dev,
  OUT UINT8                 Buf[7]
  )
{
  EFI_USB_DEVICE_REQUEST  Req;
  UINT32                  UsbStatus;
  EFI_STATUS              Status;

  if ((Dev->Quirks & PL2303_QUIRK_NO_BREAK_GETLINE) != 0) {
    CopyMem (Buf, Dev->LineSettings, 7);
    return EFI_SUCCESS;
  }

  Req.RequestType = GET_LINE_REQUEST_TYPE;
  Req.Request     = GET_LINE_REQUEST;
  Req.Value       = 0;
  Req.Index       = 0;
  Req.Length      = 7;

  Status = Dev->UsbIo->UsbControlTransfer (
                         Dev->UsbIo,
                         &Req,
                         EfiUsbDataIn,
                         100,
                         Buf,
                         7,
                         &UsbStatus
                         );
  if (EFI_ERROR (Status) || (UsbStatus != 0)) {
    DEBUG ((DEBUG_ERROR, "PL2303: GetLineRequest failed: %r\n", Status));
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}

/**
  Write a 7-byte CDC line-coding to the device and cache it.

  @param[in]  Dev  Driver private data.
  @param[in]  Buf  Seven-byte line-coding to send.

  @retval EFI_SUCCESS       Transfer succeeded.
  @retval EFI_DEVICE_ERROR  Transfer failed.
**/
STATIC
EFI_STATUS
Pl2303SetLineRequest (
  IN PL2303_PRIVATE_DATA  *Dev,
  IN UINT8                 Buf[7]
  )
{
  EFI_USB_DEVICE_REQUEST  Req;
  UINT32                  UsbStatus;
  EFI_STATUS              Status;

  Req.RequestType = SET_LINE_REQUEST_TYPE;
  Req.Request     = SET_LINE_REQUEST;
  Req.Value       = 0;
  Req.Index       = 0;
  Req.Length      = 7;

  Status = Dev->UsbIo->UsbControlTransfer (
                         Dev->UsbIo,
                         &Req,
                         EfiUsbDataOut,
                         100,
                         Buf,
                         7,
                         &UsbStatus
                         );
  if (EFI_ERROR (Status) || (UsbStatus != 0)) {
    DEBUG ((DEBUG_ERROR, "PL2303: SetLineRequest failed: %r\n", Status));
    return EFI_DEVICE_ERROR;
  }
  CopyMem (Dev->LineSettings, Buf, 7);
  return EFI_SUCCESS;
}

/* =========================================================================
 * Baud-rate encoding  (ported from the Linux driver)
 * ========================================================================= */

/**
  Return the nearest baud rate that the PL2303 supports via the direct
  (integer) encoding method.

  @param[in]  Baud  Requested baud rate.
  @return     Closest supported rate.
**/
STATIC
UINT32
Pl2303GetSupportedBaudRate (
  IN UINT32  Baud
  )
{
  STATIC CONST UINT32 BaudSup[] = {
    75, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600,
    14400, 19200, 28800, 38400, 57600, 115200, 230400, 460800,
    614400, 921600, 1228800, 2457600, 3000000, 6000000
  };
  UINTN  i;

  for (i = 0; i < ARRAY_SIZE (BaudSup); i++) {
    if (BaudSup[i] > Baud) {
      break;
    }
  }

  if (i == ARRAY_SIZE (BaudSup)) {
    return BaudSup[i - 1];
  }
  if ((i > 0) && ((BaudSup[i] - Baud) > (Baud - BaudSup[i - 1]))) {
    return BaudSup[i - 1];
  }
  return BaudSup[i];
}

/**
  Encode a baud rate as a little-endian 32-bit integer into Buf[0..3].

  @param[in]  Baud  Baud rate to encode.
  @param[out] Buf   Four bytes to fill.
  @return     The actual baud rate encoded (same as Baud for direct method).
**/
STATIC
UINT32
Pl2303EncodeBaudRateDirect (
  IN  UINT32  Baud,
  OUT UINT8   Buf[4]
  )
{
  Buf[0] = (UINT8)( Baud        & 0xFF);
  Buf[1] = (UINT8)((Baud >>  8) & 0xFF);
  Buf[2] = (UINT8)((Baud >> 16) & 0xFF);
  Buf[3] = (UINT8)((Baud >> 24) & 0xFF);
  return Baud;
}

/**
  Encode a baud rate using the standard HX divisor method into Buf[0..3].
  Formula: baudrate = 12M * 32 / (mantissa * 4^exponent)

  @param[in]  Baud  Requested baud rate.
  @param[out] Buf   Four bytes to fill.
  @return     The actual baud rate achievable with this encoding.
**/
STATIC
UINT32
Pl2303EncodeBaudRateDivisor (
  IN  UINT32  Baud,
  OUT UINT8   Buf[4]
  )
{
  UINT32  Baseline;
  UINT32  Mantissa;
  UINT32  Exponent;

  Baseline = 12000000 * 32;
  Mantissa = Baseline / Baud;
  if (Mantissa == 0) {
    Mantissa = 1;
  }
  Exponent = 0;
  while (Mantissa >= 512) {
    if (Exponent < 7) {
      Mantissa >>= 2;
      Exponent++;
    } else {
      Mantissa = 511;
      break;
    }
  }

  Buf[3] = 0x80;
  Buf[2] = 0;
  Buf[1] = (UINT8)((Exponent << 1) | (Mantissa >> 8));
  Buf[0] = (UINT8)(Mantissa & 0xFF);

  return (Baseline / Mantissa) >> (Exponent << 1);
}

/**
  Encode a baud rate using the alternate TA/TB divisor method into Buf[0..3].
  Formula: baudrate = 12M * 32 / (mantissa * 2^exponent)

  @param[in]  Baud  Requested baud rate.
  @param[out] Buf   Four bytes to fill.
  @return     The actual baud rate achievable with this encoding.
**/
STATIC
UINT32
Pl2303EncodeBaudRateDivisorAlt (
  IN  UINT32  Baud,
  OUT UINT8   Buf[4]
  )
{
  UINT32  Baseline;
  UINT32  Mantissa;
  UINT32  Exponent;

  Baseline = 12000000 * 32;
  Mantissa = Baseline / Baud;
  if (Mantissa == 0) {
    Mantissa = 1;
  }
  Exponent = 0;
  while (Mantissa >= 2048) {
    if (Exponent < 15) {
      Mantissa >>= 1;
      Exponent++;
    } else {
      Mantissa = 2047;
      break;
    }
  }

  Buf[3] = 0x80;
  Buf[2] = (UINT8)(Exponent & 0x01);
  Buf[1] = (UINT8)(((Exponent & 0x0E) << 4) | (Mantissa >> 8));
  Buf[0] = (UINT8)(Mantissa & 0xFF);

  return (Baseline / Mantissa) >> Exponent;
}

/**
  Choose the appropriate baud-rate encoding for this chip type and write
  the result into Buf[0..3].

  @param[in]  Dev   Driver private data (type and quirks are consulted).
  @param[in]  Baud  Requested baud rate.
  @param[out] Buf   Four bytes to fill.
  @return     The actual baud rate that will be programmed.
**/
STATIC
UINT32
Pl2303EncodeBaudRate (
  IN  PL2303_PRIVATE_DATA  *Dev,
  IN  UINT32                Baud,
  OUT UINT8                 Buf[4]
  )
{
  CONST PL2303_TYPE_DATA  *TypeData;
  UINT32                   BaudSup;

  TypeData = &Pl2303TypeData[Dev->Type];

  /* Cap to the maximum the chip can handle */
  if ((TypeData->MaxBaudRate != 0) && (Baud > TypeData->MaxBaudRate)) {
    Baud = TypeData->MaxBaudRate;
  }

  if (TypeData->NoDivisors) {
    /* HXN and similar: only direct encoding is supported */
    BaudSup = Baud;
  } else {
    BaudSup = Pl2303GetSupportedBaudRate (Baud);
  }

  if (Baud == BaudSup) {
    return Pl2303EncodeBaudRateDirect (Baud, Buf);
  }
  if (TypeData->AltDivisors) {
    return Pl2303EncodeBaudRateDivisorAlt (Baud, Buf);
  }
  return Pl2303EncodeBaudRateDivisor (Baud, Buf);
}

/* =========================================================================
 * Receive ring-buffer helpers
 * ========================================================================= */

/** Number of bytes currently stored in the receive ring buffer. */
STATIC
UINTN
RxAvail (
  IN CONST PL2303_PRIVATE_DATA  *Dev
  )
{
  return (Dev->RxTail + PL2303_MAX_RX_BUFFER - Dev->RxHead) % PL2303_MAX_RX_BUFFER;
}

/**
  Append up to Len bytes from Src into the receive ring buffer.
  Bytes that would overflow the buffer are silently dropped.

  @param[in,out]  Dev  Driver private data.
  @param[in]      Src  Source data.
  @param[in]      Len  Number of bytes to add.
**/
STATIC
VOID
RxEnqueue (
  IN OUT PL2303_PRIVATE_DATA  *Dev,
  IN     CONST UINT8          *Src,
  IN     UINTN                 Len
  )
{
  UINTN  Free;
  UINTN  i;

  Free = PL2303_MAX_RX_BUFFER - 1 - RxAvail (Dev);
  if (Len > Free) {
    Len = Free;   /* drop overflow */
  }

  for (i = 0; i < Len; i++) {
    Dev->RxBuf[Dev->RxTail] = Src[i];
    Dev->RxTail = (Dev->RxTail + 1) % PL2303_MAX_RX_BUFFER;
  }
}

/**
  Consume up to Len bytes from the receive ring buffer into Dst.

  @param[in,out]  Dev  Driver private data.
  @param[out]     Dst  Destination buffer.
  @param[in]      Len  Maximum bytes to consume.
  @return         Actual number of bytes copied.
**/
STATIC
UINTN
RxDequeue (
  IN OUT PL2303_PRIVATE_DATA  *Dev,
  OUT    UINT8                *Dst,
  IN     UINTN                 Len
  )
{
  UINTN  Avail;
  UINTN  i;

  Avail = RxAvail (Dev);
  if (Len > Avail) {
    Len = Avail;
  }

  for (i = 0; i < Len; i++) {
    Dst[i] = Dev->RxBuf[Dev->RxHead];
    Dev->RxHead = (Dev->RxHead + 1) % PL2303_MAX_RX_BUFFER;
  }
  return Len;
}

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
  UINT8                 StatusBuf[10];
  UINTN                 Len;
  UINT32                UsbStatus;

  Dev = PL2303_FROM_SERIAL_IO (This);

  /* Attempt a non-blocking synchronous interrupt poll to update line status */
  if (Dev->IntInAddr != 0) {
    Len = sizeof (StatusBuf);
    if (!EFI_ERROR (Dev->UsbIo->UsbSyncInterruptTransfer (
                                  Dev->UsbIo,
                                  Dev->IntInAddr,
                                  StatusBuf,
                                  &Len,
                                  1,          /* 1 ms timeout */
                                  &UsbStatus
                                  ))) {
      UINTN StatusIndex = ((Dev->Quirks & PL2303_QUIRK_UART_STATE_IDX0) != 0)
                          ? 0 : UART_STATE_INDEX;
      if (Len > StatusIndex) {
        Dev->LineStatus = StatusBuf[StatusIndex];
      }
    }
  }

  /* Also try a non-blocking bulk-IN poll to keep the rx buffer topped up */
  if (Dev->BulkInAddr != 0) {
    UINT8  BulkInBuffer[PL2303_MAX_XFER_SIZE];
    Len = sizeof (BulkInBuffer);
    if (!EFI_ERROR (Dev->UsbIo->UsbBulkTransfer (
                                  Dev->UsbIo,
                                  Dev->BulkInAddr,
                                  BulkInBuffer,
                                  &Len,
                                  1,      /* 1 ms */
                                  &UsbStatus
                                  )) && (Len > 0)) {
      RxEnqueue (Dev, BulkInBuffer, Len);
    }
  }

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
  EFI_STATUS            Status;
  EFI_USB_IO_PROTOCOL  *UsbIo;
  PL2303_PRIVATE_DATA  *Dev;
  UINTN                 Quirks;
  INT32                 Type;

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
    DEBUG ((DEBUG_ERROR, "PL2303: failed to detect chip type\n"));
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

  DEBUG ((DEBUG_INFO, "PL2303: detected type %a, quirks 0x%lx\n",
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
  Dev->Mode.Timeout          = 1000000;  /* 1 second in microseconds */
  Dev->Mode.BaudRate         = 115200;
  Dev->Mode.ReceiveFifoDepth = 16;
  Dev->Mode.DataBits         = 8;
  Dev->Mode.Parity           = NoParity;
  Dev->Mode.StopBits         = OneStopBit;

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
  gPl2303DriverBinding.DriverBindingHandle = ImageHandle;
  gPl2303DriverBinding.ImageHandle         = ImageHandle;

  return gBS->InstallMultipleProtocolInterfaces (
                &gPl2303DriverBinding.DriverBindingHandle,
                &gEfiDriverBindingProtocolGuid,
                &gPl2303DriverBinding,
                NULL
                );
}
