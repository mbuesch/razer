#ifndef RAZER_HW_BOOMSLANGCE_H_
#define RAZER_HW_BOOMSLANGCE_H_

#include "razer_private.h"


int razer_boomslangce_init(struct razer_mouse *m,
			   struct libusb_device *udev);
void razer_boomslangce_release(struct razer_mouse *m);

#endif /* RAZER_HW_BOOMSLANGCE_H_ */
