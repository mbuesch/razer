#ifndef RAZER_HW_COPPERHEAD_H_
#define RAZER_HW_COPPERHEAD_H_

#include "librazer.h"

struct usb_device;

void razer_copperhead_gen_idstr(struct usb_device *udev, char *buf);
int razer_copperhead_init_struct(struct razer_mouse *m,
				 struct usb_device *usbdev);
void razer_copperhead_release(struct razer_mouse *m);
void razer_copperhead_assign_usb_device(struct razer_mouse *m,
					struct usb_device *usbdev);

#endif /* RAZER_HW_COPPERHEAD_H_ */
