#ifndef RAZER_HW_DEATHADDER_H_
#define RAZER_HW_DEATHADDER_H_

#include "librazer.h"

struct usb_device;

int razer_deathadder_init(struct razer_mouse *m,
			  struct usb_device *usbdev);
void razer_deathadder_release(struct razer_mouse *m);

void razer_deathadder_gen_idstr(struct usb_device *udev, char *buf);

void razer_deathadder_assign_usb_device(struct razer_mouse *m,
					struct usb_device *usbdev);

#endif /* RAZER_HW_DEATHADDER_H_ */
