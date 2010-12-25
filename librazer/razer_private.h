#ifndef RAZER_PRIVATE_H_
#define RAZER_PRIVATE_H_

#include "librazer.h"
#include "util.h"

#include <usb.h>
#include <stdio.h>


extern razer_logfunc_t razer_logfunc_info;
extern razer_logfunc_t razer_logfunc_error;
extern razer_logfunc_t razer_logfunc_debug;

#define call_razer_logfunc(func, ...)	do {		\
		if (func)				\
			func("librazer: " __VA_ARGS__);	\
	} while (0)

#define razer_info(...)		call_razer_logfunc(razer_logfunc_info, __VA_ARGS__)
#define razer_error(...)	call_razer_logfunc(razer_logfunc_error, __VA_ARGS__)
#define razer_debug(...)	call_razer_logfunc(razer_logfunc_debug, __VA_ARGS__)




#define for_each_usbbus(bus, buslist) \
	for (bus = buslist; bus; bus = bus->next)
#define for_each_usbdev(dev, devlist) \
	for (dev = devlist; dev; dev = dev->next)


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
int razer_generic_usb_claim_refcount(struct razer_usb_context *ctx,
				     unsigned int *refcount);
void razer_generic_usb_release(struct razer_usb_context *ctx);
void razer_generic_usb_release_refcount(struct razer_usb_context *ctx,
					unsigned int *refcount);

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

#define BUSTYPESTR_USB		"USB"
#define DEVTYPESTR_MOUSE	"Mouse"
static inline void razer_create_idstr(char *buf,
				      const char *bustype,
				      const char *busposition,
				      const char *devtype,
				      const char *devname,
				      const char *devid)
{
	snprintf(buf, RAZER_IDSTR_MAX_SIZE, "%s:%s:%s-%s:%s",
		 devtype, devname, bustype, busposition, devid);
}

#endif /* RAZER_PRIVATE_H_ */
