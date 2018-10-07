#include "sam4s_usb_descriptors.h"
#include <stdlib.h>

/*
 * NOTE: WE ASSUME THAT THE SYSTEM EXECUTING THE CODE
 *       IS LITTLE ENDIAN! (16bit values in descriptors
 *       are little endian, and we don't convert!)
 */

const struct libusb_device_descriptor sam4s_usb_descr_dev = {
	.bLength = sizeof(sam4s_usb_descr_dev),
	.bDescriptorType = LIBUSB_DT_DEVICE,
	.bcdUSB = 0x0100,  /* 1.00 -> no high-speed */
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64, /* endpoint 0 max packet size */
	.idVendor = 0x1d50,  /* OpenMoko, see registry repository at... */
	.idProduct = 0x613b, /* https://github.com/openmoko/openmoko-usb-oui */
	/* TODO: still not registered! */
	.bcdDevice = 0x0100, /* 1.00 */
	.iManufacturer = 0,
	.iProduct = 0,
	.iSerialNumber = 0,
	.bNumConfigurations = 1,
};

const struct libusb_config_descriptor sam4s_usb_descr_cfg = {
	.bLength = sizeof(sam4s_usb_descr_cfg),
	.bDescriptorType = LIBUSB_DT_CONFIG,
	.wTotalLength = sizeof(sam4s_usb_descr_cfg)+
		sizeof(sam4s_usb_descr_int)+
		sizeof(sam4s_usb_descr_ep1)+
		sizeof(sam4s_usb_descr_ep2),
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = (1<<7), /* XXX */
	.bMaxPower = 250, /* 500 mA in units of 2mA */
};

const struct libusb_interface_descriptor sam4s_usb_descr_int = {
	.bLength = sizeof(sam4s_usb_descr_int),
	.bDescriptorType = LIBUSB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = 0xff,     /* vendor specific */
	.bInterfaceSubClass = 0xff,  /* vendor specific */
	.bInterfaceProtocol = 0xff,  /* vendor specific */
	.iInterface =0,
};

const struct libusb_endpoint_descriptor sam4s_usb_descr_ep1 = {
	.bLength = sizeof(sam4s_usb_descr_ep1),
	.bDescriptorType = LIBUSB_DT_ENDPOINT,
	/* 0x80 = IN(1),OUT(0), D0..D3: ep number, D6..D4: reserved(0) */
	.bEndpointAddress = 0x84, /* EP4, IN */
 	/* D1..0: xfer type: 0=control, 1=isochonous, 2=bulk, 3=interrupt,
	   D3..2: isochr:    0=no sync, 1=async, 2=adaptive, 3=synchronous,
	   D5..4: usage:     0=data, 1=feedback, 1:implicit feedback, 3: rsvd */
	.bmAttributes = 0x0a, /* isochronous, adaptive */
	.wMaxPacketSize = 512,
	.bInterval = 1,
};

const struct libusb_endpoint_descriptor sam4s_usb_descr_ep2 = {
	.bLength = sizeof(sam4s_usb_descr_ep1),
	.bDescriptorType = LIBUSB_DT_ENDPOINT,
	.bEndpointAddress = 0x05, /* EP5 OUT */
	.bmAttributes = 0x0a, /* isochronous, adaptive */
	.wMaxPacketSize = 512,
	.bInterval = 1,
};
