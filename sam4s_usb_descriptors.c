#include "sam4s_usb_descriptors.h"
#include <stdlib.h>

/*
 * NOTE: WE ASSUME THAT THE SYSTEM EXECUTING THE CODE
 *       IS LITTLE ENDIAN! (16bit values in descriptors
 *       are little endian, and we don't convert!)
 */

#define DESCRIPTOR_TYPE_DEV 0x04

const struct libusb_device_descriptor sam4s_usb_descr_dev = {
	.bLength = sizeof(sam4s_usb_descr_dev),
	.bDescriptorType = LIBUSB_DT_DEVICE,
	.bcdUSB = 2,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64, /* endpoint 0 max packet size */
	.idVendor = 0x1245,
	.idProduct = 0x2342,
	.bcdDevice = 2,
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

struct libusb_descriptor_common_hdr {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
} __attribute__((packed));

const void * sam4s_usb_descriptors_table[] = {
	&sam4s_usb_descr_dev,
	&sam4s_usb_descr_cfg,
};

const void *
sam4s_usb_descriptors_get(enum libusb_descriptor_type dt, int num)
{
	const void **p = sam4s_usb_descriptors_table;
	const struct libusb_descriptor_common_hdr *pdch;

	int i = 1;

	for (p = sam4s_usb_descriptors_table; *p != NULL; p++) {
		pdch = (const struct libusb_descriptor_common_hdr*)p;
		if (pdch->bDescriptorType != dt)
			continue;
		if (num == i)
			return (void*)pdch;
		i++;
	}

	return NULL;
}
