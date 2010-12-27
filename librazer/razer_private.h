#ifndef RAZER_PRIVATE_H_
#define RAZER_PRIVATE_H_

#include "librazer.h"
#include "util.h"

#include <libusb.h>
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


struct razer_usb_interface {
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
};

#define RAZER_MAX_NR_INTERFACES		2

struct razer_usb_context {
	/* Device pointer. */
	struct libusb_device *dev;
	/* The handle for all operations. */
	struct libusb_device_handle *h;
	/* The configuration we want to use. Defaults to 1. */
	uint8_t bConfigurationValue;
	/* The interfaces we use. */
	struct razer_usb_interface interfaces[RAZER_MAX_NR_INTERFACES];
	unsigned int nr_interfaces;
};

int razer_usb_add_used_interface(struct razer_usb_context *ctx,
				 int bInterfaceNumber,
				 int bAlternateSetting);

int razer_generic_usb_claim(struct razer_usb_context *ctx);
int razer_generic_usb_claim_refcount(struct razer_usb_context *ctx,
				     unsigned int *refcount);
void razer_generic_usb_release(struct razer_usb_context *ctx);
void razer_generic_usb_release_refcount(struct razer_usb_context *ctx,
					unsigned int *refcount);

struct razer_usb_reconnect_guard {
	struct razer_usb_context *ctx;
	struct libusb_device_descriptor old_desc;
	uint8_t old_busnr;
	uint8_t old_devaddr;
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

void razer_generic_usb_gen_idstr(struct libusb_device *udev,
				 struct libusb_device_handle *h,
				 const char *devname,
				 bool include_devicenr,
				 char *idstr_buf);

void razer_init_axes(struct razer_axis *axes,
		     const char *name0, unsigned int flags0,
		     const char *name1, unsigned int flags1,
		     const char *name2, unsigned int flags2);

#endif /* RAZER_PRIVATE_H_ */
