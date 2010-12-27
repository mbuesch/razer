#ifndef RAZER_HW_NAGA_H_
#define RAZER_HW_NAGA_H_

#include "razer_private.h"


int razer_naga_init(struct razer_mouse *m,
		    struct libusb_device *udev);
void razer_naga_release(struct razer_mouse *m);

#endif /* RAZER_HW_NAGA_H_ */
