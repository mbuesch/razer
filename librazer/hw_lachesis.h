#ifndef RAZER_HW_LACHESIS_H_
#define RAZER_HW_LACHESIS_H_

#include "librazer.h"

struct usb_device;

int razer_lachesis_init(struct razer_mouse *m,
			struct usb_device *usbdev);
void razer_lachesis_release(struct razer_mouse *m);

void razer_lachesis_gen_idstr(struct usb_device *udev, char *buf);

void razer_lachesis_assign_usb_device(struct razer_mouse *m,
				      struct usb_device *usbdev);

#endif /* RAZER_HW_LACHESIS_H_ */
