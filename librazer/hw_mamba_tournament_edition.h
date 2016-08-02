 
#ifndef RAZER_HW_MAMBA_TE_H_
#define RAZER_HW_MAMBA_TE_H_

#include "razer_private.h"


int razer_mamba_te_init(struct razer_mouse *m,
			 struct libusb_device *udev);
void razer_mamba_te_release(struct razer_mouse *m);

#endif /* RAZER_MAMBA_TOURNAMENT_EDITION_ */
