#ifndef RAZER_PROFILE_EMULATION_H_
#define RAZER_PROFILE_EMULATION_H_

#include "razer_private.h"


#define PROFEMU_NAME_MAX	32

struct razer_mouse_profile_emu_data {
	/* Profile name string */
	razer_utf16_t name[PROFEMU_NAME_MAX + 1];
	/* Frequency selection for this emulated profile */
	enum razer_mouse_freq freq;
	/* DPI mappings (per axis) for this emulated profile */
	struct razer_mouse_dpimapping *dpimappings[3];
	unsigned int nr_dpimappings;
	/* Button mappings (per physical button) for this emulated profile */
	struct razer_button_function *butfuncs[11];
	unsigned int nr_butfuncs;
};

struct razer_mouse_profile_emu {
	struct razer_mouse *mouse;
	/* Emulated profiles */
	struct razer_mouse_profile profiles[RAZER_NR_EMULATED_PROFILES];
	struct razer_mouse_profile_emu_data data[RAZER_NR_EMULATED_PROFILES];
	struct razer_mouse_profile *active_profile;
	/* The hardware profile. This is what the driver uses. */
	struct razer_mouse_profile *hw_profile;
};


int razer_mouse_init_profile_emulation(struct razer_mouse *m);
void razer_mouse_exit_profile_emulation(struct razer_mouse *m);

#endif /* RAZER_PROFILE_EMULATION_H_ */
