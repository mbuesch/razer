#ifndef RAZER_HW_KRAIT_H_
#define RAZER_HW_KRAIT_H_

#include "razer_private.h"


int razer_krait_init(struct razer_mouse *m,
		     struct libusb_device *udev);
void razer_krait_release(struct razer_mouse *m);

#endif /* RAZER_HW_KRAIT_H_ */
