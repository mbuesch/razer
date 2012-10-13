#ifndef RAZER_HW_IMPERATOR_H_
#define RAZER_HW_IMPERATOR_H_

#include "razer_private.h"


int razer_imperator_init(struct razer_mouse *m,
			 struct libusb_device *udev);
void razer_imperator_release(struct razer_mouse *m);

#endif /* RAZER_HW_IMPERATOR_H_ */
