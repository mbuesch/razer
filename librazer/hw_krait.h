#ifndef RAZER_HW_KRAIT_H_
#define RAZER_HW_KRAIT_H_

#include "librazer.h"

struct usb_device;

void razer_krait_gen_idstr(struct usb_device *udev, char *buf);
int razer_krait_init_struct(struct razer_mouse *m,
			    struct usb_device *usbdev);
void razer_krait_release(struct razer_mouse *m);

#endif /* RAZER_HW_KRAIT_H_ */
