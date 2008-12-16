#ifndef RAZER_HW_DEATHADDER_H_
#define RAZER_HW_DEATHADDER_H_

#include "librazer.h"

struct usb_device;

void razer_deathadder_gen_idstr(struct usb_device *udev, char *buf);
int razer_deathadder_init_struct(struct razer_mouse *m,
				 struct usb_device *usbdev);
void razer_deathadder_release(struct razer_mouse *m);

#endif /* RAZER_HW_DEATHADDER_H_ */
