#ifndef RAZER_PRIVATE_H_
#define RAZER_PRIVATE_H_

typedef _Bool bool;

struct usb_device;
struct usb_dev_handle;

struct razer_usb_context {
	/* The handle for all operations. */
	struct usb_dev_handle *h;
	/* The interface number we use. */
	int interf;
	/* Did we detach the kernel driver? */
	bool kdrv_detached;
};

int razer_generic_usb_claim(struct usb_device *dev,
			    struct razer_usb_context *ctx);
void razer_generic_usb_release(struct usb_device *dev,
			       struct razer_usb_context *ctx);

#endif /* RAZER_PRIVATE_H_ */
