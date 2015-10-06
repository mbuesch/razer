#ifndef RAZER_HW_NAGA_H_
#define RAZER_HW_NAGA_H_

#include "razer_private.h"


#define RAZER_NAGA_PID_CLASSIC  0x0015
#define RAZER_NAGA_PID_EPIC     0x001F
#define RAZER_NAGA_PID_2012     0x002e
#define RAZER_NAGA_PID_HEX      0x0036
#define RAZER_NAGA_PID_2014     0x0040
#define RAZER_NAGA_PID_HEX_2014 0x0041


int razer_naga_init(struct razer_mouse *m,
		    struct libusb_device *udev);
void razer_naga_release(struct razer_mouse *m);

#endif /* RAZER_HW_NAGA_H_ */
