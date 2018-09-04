#ifndef SAM4S_USB_DESCRIPTORS_H
#define SAM4S_USB_DESCRIPTORS_H

#include <stdint.h>

enum libusb_descriptor_type {
	LIBUSB_DT_DEVICE = 0x01,
	LIBUSB_DT_CONFIG = 0x02,
	LIBUSB_DT_STRING = 0x03,
	LIBUSB_DT_INTERFACE = 0x04,
	LIBUSB_DT_ENDPOINT = 0x05,
	LIBUSB_DT_BOS = 0x0f,
	LIBUSB_DT_DEVICE_CAPABILITY = 0x10,
	LIBUSB_DT_HID = 0x21,
	LIBUSB_DT_REPORT = 0x22,
	LIBUSB_DT_PHYSICAL = 0x23,
	LIBUSB_DT_HUB = 0x29,
	LIBUSB_DT_SUPERSPEED_HUB = 0x2a,
	LIBUSB_DT_SS_ENDPOINT_COMPANION = 0x30
};

/* from libusb */
struct libusb_device_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdUSB;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer;
	uint8_t  iProduct;
	uint8_t  iSerialNumber;
	uint8_t  bNumConfigurations;
} __attribute__((packed));

struct libusb_endpoint_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bEndpointAddress;
	uint8_t  bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t  bInterval;
	uint8_t  bRefresh;
	uint8_t  bSynchAddress;
} __attribute__((packed));

struct libusb_interface_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bInterfaceNumber;
	uint8_t  bAlternateSetting;
	uint8_t  bNumEndpoints;
	uint8_t  bInterfaceClass;
	uint8_t  bInterfaceSubClass;
	uint8_t  bInterfaceProtocol;
	uint8_t  iInterface;
} __attribute__((packed));

struct libusb_config_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t wTotalLength;
	uint8_t  bNumInterfaces;
	uint8_t  bConfigurationValue;
	uint8_t  iConfiguration;
	uint8_t  bmAttributes;
	uint8_t  bMaxPower;
} __attribute__((packed));

extern const void * sam4s_usb_descriptors_table[];

/* return descriptor number num (starting at 1) of type dt,
   return NULL if not found */
extern const void *
sam4s_usb_descriptors_get(enum libusb_descriptor_type dt, int num);

#endif