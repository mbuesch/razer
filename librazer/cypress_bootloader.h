#ifndef CYPRESS_BOOTLOADER_H_
#define CYPRESS_BOOTLOADER_H_

#include "razer_private.h"


struct cypress {
	struct razer_usb_context usb;
	unsigned int ep_in;
	unsigned int ep_out;
	void (*assign_key)(uint8_t *key);
};

#define CYPRESS_BOOT_VENDORID	0x04B4
#define CYPRESS_BOOT_PRODUCTID	0xE006


/** is_cypress_bootloader - Check whether an USB device is a cypress bootloader. */
static inline bool is_cypress_bootloader(struct libusb_device_descriptor *desc)
{
	return (desc->idVendor == CYPRESS_BOOT_VENDORID &&
		desc->idProduct == CYPRESS_BOOT_PRODUCTID);
}

/** cypress_open - Open a device.
 * @c: context structure.
 * @dev: USB device to use (must be a cypress bootloader device).
 * @assign_key: Callback function to assign the 8-byte bootloader key.
 *              If NULL, it uses the default key.
 */
int cypress_open(struct cypress *c, struct libusb_device *dev,
		 void (*assign_key)(uint8_t *key));

/** cypress_close - Close a device. */
void cypress_close(struct cypress *c);

/** cypress_upload_image - Upload a firmware image to the device.
 * The device must be opened. */
int cypress_upload_image(struct cypress *c,
			 const char *image, size_t len);


#endif /* CYPRESS_BOOTLOADER_H_ */
