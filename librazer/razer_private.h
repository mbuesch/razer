#ifndef RAZER_PRIVATE_H_
#define RAZER_PRIVATE_H_

#include <usb.h>

typedef _Bool bool;

struct razer_usb_context {
	/* Device pointer. */
	struct usb_device *dev;
	/* The handle for all operations. */
	struct usb_dev_handle *h;
	/* The interface number we use. */
	int interf;
	/* Did we detach the kernel driver? */
	bool kdrv_detached;
};

int razer_generic_usb_claim(struct razer_usb_context *ctx);
void razer_generic_usb_release(struct razer_usb_context *ctx);

struct razer_usb_reconnect_guard {
	struct razer_usb_context *ctx;
	struct usb_device_descriptor old_desc;
	char old_dirname[PATH_MAX + 1];
	char old_filename[PATH_MAX + 1];
};

int razer_usb_reconnect_guard_init(struct razer_usb_reconnect_guard *guard,
				   struct razer_usb_context *ctx);
int razer_usb_reconnect_guard_wait(struct razer_usb_reconnect_guard *guard);

void razer_msleep(unsigned int msecs);

#endif /* RAZER_PRIVATE_H_ */
