#ifndef RAZER_SYNAPSE_H_
#define RAZER_SYNAPSE_H_

#include "librazer.h"

#include <stdint.h>


enum razer_synapse_features {
	RAZER_SYNAPSE_VER0,
};

int razer_synapse_init(struct razer_mouse *m,
		       unsigned int features);
void razer_synapse_exit(struct razer_mouse *m);

const char * razer_synapse_get_serial(struct razer_mouse *m);

#endif /* RAZER_SYNAPSE_H_ */
