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
	.bcdUSB = 0x0200,  /* 2.00 */
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64, /* endpoint 0 max packet size */
	.idVendor = 0x1245,
	.idProduct = 0x2342,
	.bcdDevice = 0x0100, /* 1.00 */
	.iManufacturer = 0,
	.iProduct = 0,
	.iSerialNumber = 0,
	.bNumConfigurations = 1,
};

const struct libusb_config_descriptor sam4s_usb_descr_cfg = {
	.bLength = sizeof(sam4s_usb_descr_cfg),
	.bDescriptorType = LIBUSB_DT_CONFIG,
	.wTotalLength = sizeof(sam4s_usb_descr_cfg),
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = (1<<7), /* XXX */
	.bMaxPower = 250, /* 500 mA in units of 2mA */
};

const struct libusb_interface_descriptor sam4s_usb_descr_int = {
	.bLength = sizeof(sam4s_usb_descr_int),
	.bDescriptorType = LIBUSB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 1,
	.bNumEndpoints = 2,
	.bInterfaceClass = 0,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface =0,
};

const struct libusb_endpoint_descriptor sam4s_usb_descr_ep1 = {
	.bLength = sizeof(sam4s_usb_descr_ep1),
	.bDescriptorType = LIBUSB_DT_ENDPOINT,
	.bEndpointAddress = 1,
	.bmAttributes = 0,
	.wMaxPacketSize = 128,
	.bInterval = 0,
	.bRefresh = 0,
	.bSynchAddress = 0
};

const struct libusb_endpoint_descriptor sam4s_usb_descr_ep2 = {
	.bLength = sizeof(sam4s_usb_descr_ep1),
	.bDescriptorType = LIBUSB_DT_ENDPOINT,
	.bEndpointAddress = 2,
	.bmAttributes = 0,
	.wMaxPacketSize = 128,
	.bInterval = 0,
	.bRefresh = 0,
	.bSynchAddress = 0
};