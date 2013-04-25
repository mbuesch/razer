#ifndef RAZER_HW_TAIPAN_H_
#define RAZER_HW_TAIPAN_H_

#include "razer_private.h"


int razer_taipan_init(struct razer_mouse *m,
		    struct libusb_device *udev);
void razer_taipan_release(struct razer_mouse *m);

#endif /* RAZER_HW_TAIPAN_H_ */
