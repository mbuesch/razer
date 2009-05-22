#ifndef RAZER_PRIVATE_H_
#define RAZER_PRIVATE_H_

#include <usb.h>
#include <stdio.h>

#define DEBUG	1 /* Enable/disable debugging */


#if DEBUG
# define dprintf(...)		printf("[librazer debug]: " __VA_ARGS__)
#else
# define dprintf		noprintf
#endif
static inline int noprintf(const char *t, ...) { return 0; }


#define for_each_usbbus(bus, buslist) \
	for (bus = buslist; bus; bus = bus->next)
#define for_each_usbdev(dev, devlist) \
	for (dev = devlist; dev; dev = dev->next)


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
int razer_usb_reconnect_guard_wait(struct razer_usb_reconnect_guard *guard, bool hub_reset);

int razer_usb_force_reinit(struct razer_usb_context *ctx);

void razer_msleep(unsigned int msecs);

#endif /* RAZER_PRIVATE_H_ */
