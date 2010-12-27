#ifndef RAZER_HW_COPPERHEAD_H_
#define RAZER_HW_COPPERHEAD_H_

#include "razer_private.h"


int razer_copperhead_init(struct razer_mouse *m,
			  struct libusb_device *udev);
void razer_copperhead_release(struct razer_mouse *m);

#endif /* RAZER_HW_COPPERHEAD_H_ */
