#ifndef RAZER_HW_LACHESIS_H_
#define RAZER_HW_LACHESIS_H_

#include "razer_private.h"


int razer_lachesis_init(struct razer_mouse *m,
			struct libusb_device *udev);
void razer_lachesis_release(struct razer_mouse *m);

#endif /* RAZER_HW_LACHESIS_H_ */
