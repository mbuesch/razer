#ifndef RAZER_HW_NAGA_H_
#define RAZER_HW_NAGA_H_

#include "librazer.h"

struct usb_device;

int razer_naga_init(struct razer_mouse *m,
		    struct usb_device *usbdev);
void razer_naga_release(struct razer_mouse *m);

void razer_naga_gen_idstr(struct usb_device *udev, char *buf);

void razer_naga_assign_usb_device(struct razer_mouse *m,
				  struct usb_device *usbdev);

#endif /* RAZER_HW_NAGA_H_ */
