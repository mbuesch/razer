#ifndef CYPRESS_BOOTLOADER_H_
#define CYPRESS_BOOTLOADER_H_

#include "razer_private.h"


struct cypress {
	struct razer_usb_context usb;
	unsigned int interface;
};

#define CYPRESS_BOOT_VENDORID	0x04B4
#define CYPRESS_BOOT_PRODUCTID	0xE006


int cypress_open(struct cypress *c, struct usb_device *dev);
void cypress_close(struct cypress *c);

int cypress_upload_image(struct cypress *c,
			 const char *image, size_t len);


#endif /* CYPRESS_BOOTLOADER_H_ */
