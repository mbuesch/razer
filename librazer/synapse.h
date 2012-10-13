#ifndef RAZER_SYNAPSE_H_
#define RAZER_SYNAPSE_H_

#include "librazer.h"

#include <stdint.h>


enum razer_synapse_features {
	RAZER_SYNFEAT_RGBLEDS	= (1 << 0),	/* RGB LEDs supported */
};

int razer_synapse_init(struct razer_mouse *m,
		       unsigned int features);
void razer_synapse_exit(struct razer_mouse *m);

int razer_synapse_set_led_name(struct razer_mouse *m,
			       unsigned int index,
			       const char *name);

const char * razer_synapse_get_serial(struct razer_mouse *m);

#endif /* RAZER_SYNAPSE_H_ */
