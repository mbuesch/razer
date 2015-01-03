#ifndef RAZER_HW_DEATHADDER_CHROMA_H_
#define RAZER_HW_DEATHADDER_CHROMA_H_

#include "razer_private.h"

int razer_deathadder_chroma_init(struct razer_mouse *m,
                                 struct libusb_device *usbdev);

void razer_deathadder_chroma_release(struct razer_mouse *m);

#endif /* RAZER_HW_DEATHADDER_CHROMA_H_ */
