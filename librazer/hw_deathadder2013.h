#ifndef RAZER_HW_DEATHADDER2013_H_
#define RAZER_HW_DEATHADDER2013_H_

#include "razer_private.h"

int razer_deathadder2013_init(struct razer_mouse *m,
			      struct libusb_device *udev);

void razer_deathadder2013_release(struct razer_mouse *m);

#endif /* RAZER_HW_DEATHADDER2013_H_ */
