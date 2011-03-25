/*
 *   Razer lowlevel device access library.
 *   Applications do NOT want to use this.
 *   Applications should use pyrazer or librazerd instead.
 *
 *   Copyright (C) 2007-2010 Michael Buesch <mb@bu3sch.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#ifndef LIB_RAZER_H_
#define LIB_RAZER_H_

#ifndef RAZERCFG_BUILD
# error "librazer.h is a razercfg internal library!"
# error "Do not include this file into your application!"
#endif

#include <stdlib.h>


#define RAZER_IDSTR_MAX_SIZE	128
#define RAZER_LEDNAME_MAX_SIZE	64
#define RAZER_DEFAULT_CONFIG	"/etc/razer.conf"

/* Opaque internal data structures */
struct razer_usb_context;
struct razer_mouse_base_ops;
struct razer_mouse_profile_emu;

struct razer_mouse;


/** enum razer_led_state - The LED state value
  * @RAZER_LED_OFF: The LED is turned off
  * @RAZER_LED_ON: The LED is turned on
  * @RAZER_LED_UNKNOWN: The LED is in an unknown state (on or off)
  */
enum razer_led_state {
	RAZER_LED_OFF		= 0,
	RAZER_LED_ON		= 1,
	RAZER_LED_UNKNOWN,
};

/** struct razer_led - A LED on a razer device.
  *
  * @next: The next LED device in the linked list.
  *
  * @name: The human readable name string for the LED.
  * @id: A unique ID cookie
  * @state: The state of the LED (on, off, unknown)
  *
  * @toggle_state: Toggle the state. Note that a new_state of
  * 	RAZER_LED_UNKNOWN result in an error.
  *
  * @u: This union contains a pointer to the parent device.
  */
struct razer_led {
	struct razer_led *next;

	const char *name;
	unsigned int id;
	enum razer_led_state state;

	int (*toggle_state)(struct razer_led *led,
			    enum razer_led_state new_state);

	union {
		struct razer_mouse *mouse;
	} u;
};

/** enum razer_mouse_freq - Mouse scan frequency
  * @RAZER_MOUSE_FREQ_UNKNOWN: Unknown scan frequency
  */
enum razer_mouse_freq {
	RAZER_MOUSE_FREQ_UNKNOWN	= 0,
	RAZER_MOUSE_FREQ_125HZ		= 125,
	RAZER_MOUSE_FREQ_500HZ		= 500,
	RAZER_MOUSE_FREQ_1000HZ		= 1000,
};

/** enum razer_mouse_res - Mouse scan resolutions
  * @RAZER_MOUSE_RES_UNKNOWN: Unknown scan resolution
  */
enum razer_mouse_res {
	RAZER_MOUSE_RES_UNKNOWN		= 0,
	RAZER_MOUSE_RES_125DPI		= 125,
	RAZER_MOUSE_RES_250DPI		= 250,
	RAZER_MOUSE_RES_400DPI		= 400,
	RAZER_MOUSE_RES_450DPI		= 450,
	RAZER_MOUSE_RES_500DPI		= 500,
	RAZER_MOUSE_RES_800DPI		= 800,
	RAZER_MOUSE_RES_900DPI		= 900,
	RAZER_MOUSE_RES_1000DPI		= 1000,
	RAZER_MOUSE_RES_1600DPI		= 1600,
	RAZER_MOUSE_RES_1800DPI		= 1800,
	RAZER_MOUSE_RES_2000DPI		= 2000,
	RAZER_MOUSE_RES_3500DPI		= 3500,
	RAZER_MOUSE_RES_4000DPI		= 4000,
};

/** enum razer_mouse_type
  * @RAZER_MOUSETYPE_DEATHADDER: A "DeathAdder" mouse
  * @RAZER_MOUSETYPE_KRAIT: A "Krait" mouse
  * @RAZER_MOUSETYPE_LACHESIS: A "Lachesis" mouse
  * @RAZER_MOUSETYPE_LACHESIS: A "Copperhead" mouse
  * @RAZER_MOUSETYPE_NAGA: A "Naga" mouse
  * @RAZER_MOUSETYPE_BOOMSLANGCE: A "Boomslang Collector's Edition" mouse
  */
enum razer_mouse_type {
	RAZER_MOUSETYPE_DEATHADDER,
	RAZER_MOUSETYPE_KRAIT,
	RAZER_MOUSETYPE_LACHESIS,
	RAZER_MOUSETYPE_COPPERHEAD,
	RAZER_MOUSETYPE_NAGA,
	RAZER_MOUSETYPE_BOOMSLANGCE,
};

/** struct razer_button_function - A logical button function
 *
 * @id: A unique ID number for the function.
 *
 * @name: A unique and human readable name string for the function.
 */
struct razer_button_function {
	unsigned int id;
	const char *name;
};

/** struct razer_button - A physical button (physical = piece of hardware)
 *
 * @id: A unique ID number for this button.
 *
 * @name: A unique and human readable name string for the button.
 */
struct razer_button {
	unsigned int id;
	const char *name;
};

/** enum razer_axis_flags - Usage flags for an axis.
 * @RAZER_AXIS_INDEPENDENT_DPIMAPPING: Supports independent DPI mappings.
 */
enum razer_axis_flags {
	RAZER_AXIS_INDEPENDENT_DPIMAPPING	= (1 << 0),
};

/** struct razer_axis - A device axis.
 *
 * @id: A unique ID number for this axis.
 *
 * @name: A unique and human readable name string for this axis.
 *
 * @flags: Usage flags.
 */
struct razer_axis {
	unsigned int id;
	const char *name;
	unsigned int flags;
};

/** struct razer_mouse_dpimapping - Mouse scan resolution mapping.
 *
 * @nr: An ID number. Read only!
 *
 * @res: The resolution value. Read only!
 *
 * @change: Change this mapping to another resolution value.
 *	May be NULL, if the mapping cannot be changed.
 */
struct razer_mouse_dpimapping {
	unsigned int nr;
	enum razer_mouse_res res;

	int (*change)(struct razer_mouse_dpimapping *d, enum razer_mouse_res res);

	struct razer_mouse *mouse;
};

/** struct razer_mouse_profile - A mouse profile
 *
 * @get_freq: Get the currently used scan frequency.
 *
 * @set_freq: Change the mouse scan frequency.
 *
 * @get_dpimapping: Returns the active scan resolution mapping.
 *	If axis is NULL, returns the mapping of the first axis.
 *
 * @set_dpimapping: Sets the active scan resolution mapping.
 *	If axis is NULL, sets the mapping of all axes.
 *
 * @get_button_function: Get the currently assigned function for a button.
 *
 * @set_button_function: Assign a new function to a button.
 */
struct razer_mouse_profile {
	unsigned int nr;

	enum razer_mouse_freq (*get_freq)(struct razer_mouse_profile *p);
	int (*set_freq)(struct razer_mouse_profile *p, enum razer_mouse_freq freq);

	struct razer_mouse_dpimapping * (*get_dpimapping)(struct razer_mouse_profile *p,
							  struct razer_axis *axis);
	int (*set_dpimapping)(struct razer_mouse_profile *p,
			      struct razer_axis *axis,
			      struct razer_mouse_dpimapping *d);

	struct razer_button_function * (*get_button_function)(struct razer_mouse_profile *p,
							      struct razer_button *b);
	int (*set_button_function)(struct razer_mouse_profile *p,
				   struct razer_button *b,
				   struct razer_button_function *f);

	struct razer_mouse *mouse;
};

/** enum razer_mouse_flags - Flags for a mouse
 *
 * @RAZER_MOUSEFLG_NEW: The device detection routine of the library
 *                      sets this to flag on detection. So the highlevel code
 *                      using the library can clear this flag to keep track of
 *                      devices it already knows about.
 *
 * @RAZER_MOUSEFLG_PROFEMU: Profiles are emulated in software. The device
 *                          does only support one profile in hardware.
 */
enum razer_mouse_flags {
	RAZER_MOUSEFLG_NEW		= (1 << 0),
	RAZER_MOUSEFLG_PROFEMU		= (1 << 1),

	/* Internal flags */
	RAZER_MOUSEFLG_PRESENT		= (1 << 15),
};

/** enum - Various constants
 *
 * @RAZER_FW_FLASH_MAGIC: Magic parameter to flash_firmware callback.
 *
 * @RAZER_NR_EMULATED_PROFILES: Default number of emulated profiles.
 */
enum {
	RAZER_FW_FLASH_MAGIC		= 0xB00B135,
	RAZER_NR_EMULATED_PROFILES	= 20,
};

/** struct razer_mouse - Representation of a mouse device
  *
  * @next: Linked list to the next mouse.
  *
  * @idstr: A system wide unique ID string for the device.
  *
  * @type: The mouse type
  *
  * @flags: Various ORed enum razer_mouse_flags.
  *
  * @claim: Claim and open the backend device (USB).
  * 	As long as the device is claimed, it is not operable by the user!
  *	Claim can be called multiple times before release, but it must always
  *	pair up with the corresponding number of release calls.
  *
  * @release: Release a claimed backend device.
  *
  * @get_fw_version: Read the firmware version from the device.
  *     Returns the firmware version or a negative error code.
  *
  * @get_leds: Get a linked list of available LEDs.
  * 	Returns the number of LEDs or a negative error code.
  * 	leds_list points to the first LED in the list.
  * 	The caller is responsible to free every item in leds_list.
  *
  * @flash_firmware: Upload a firmware image to the device and
  *     flash it to the PROM. &magic_number is &RAZER_FW_FLASH_MAGIC.
  *     The magic is used to project against accidental calls.
  *
  * @nr_profiles: The number of profiles supported by this device.
  *
  * @get_profiles: Returns an array of supported profiles.
  *	Array length is nr_profiles.
  *
  * @get_active_profile: Returns the currently active profile.
  *
  * @set_active_profile: Selects the active profile.
  *	May be NULL, if there's only one profile.
  *
  * @supported_axes: Returns a list of supported device axes
  *	for this mouse in res_ptr.
  *	The return value is a positive list length or a negative error code.
  *
  * @supported_resolutions: Returns a list of supported scan resolutions
  *	for this mouse in res_ptr.
  *	The return value is a positive list length or a negative error code.
  *
  * @supported_freqs: Get an array of supported scan frequencies.
  * 	Returns the array size or a negative error code.
  * 	freq_ptr points to the array.
  * 	The caller is responsible to free freq_ptr.
  *
  * @supported_dpimappings: Returns a list of supported scan resolution
  *	mappings in res_ptr.
  *	The function return value is the positive list size or a negative
  *	error code.
  *
  * @supported_buttons: Returns a list of physical buttons on the device
  *	in res_ptr.
  *	The function return value is the positive list size or a negative
  *	error code.
  *
  * @supported_button_functions: Returns a list of possible function assignments
  *	for the physical buttons in res_ptr.
  *	The function return value is the positive list size or a negative
  *	error code.
  */
struct razer_mouse {
	struct razer_mouse *next;

	char idstr[RAZER_IDSTR_MAX_SIZE + 1];

	enum razer_mouse_type type;
	unsigned int flags;

	int (*claim)(struct razer_mouse *m);
	void (*release)(struct razer_mouse *m);

	int (*get_fw_version)(struct razer_mouse *m);

	int (*get_leds)(struct razer_mouse *m,
			struct razer_led **leds_list);

	int (*flash_firmware)(struct razer_mouse *m,
			      const char *data, size_t len,
			      unsigned int magic_number);

	unsigned int nr_profiles;
	struct razer_mouse_profile * (*get_profiles)(struct razer_mouse *m);
	struct razer_mouse_profile * (*get_active_profile)(struct razer_mouse *m);
	int (*set_active_profile)(struct razer_mouse *m,
				  struct razer_mouse_profile *p);

	int (*supported_axes)(struct razer_mouse *m,
			      struct razer_axis **res_ptr);
	int (*supported_resolutions)(struct razer_mouse *m,
				     enum razer_mouse_res **res_ptr);
	int (*supported_freqs)(struct razer_mouse *m,
			       enum razer_mouse_freq **freq_ptr);
	int (*supported_dpimappings)(struct razer_mouse *m,
				     struct razer_mouse_dpimapping **res_ptr);
	int (*supported_buttons)(struct razer_mouse *m,
				 struct razer_button **res_ptr);
	int (*supported_button_functions)(struct razer_mouse *m,
					  struct razer_button_function **res_ptr);

	/* Do not touch these pointers. */
	const struct razer_mouse_base_ops *base_ops;
	struct razer_usb_context *usb_ctx;
	unsigned int claim_count;
	struct razer_mouse_profile_emu *profemu;
	void *drv_data; /* For use by the hardware driver */
};

/** razer_free_freq_list - Free an array of frequencies.
  * This function frees a whole array of frequencies as returned
  * by the device methods.
  */
void razer_free_freq_list(enum razer_mouse_freq *freq_list, int count);

/** razer_free_resolution_list - Free an array of resolutions.
  * This function frees a whole array of resolutions as returned
  * by the device methods.
  */
void razer_free_resolution_list(enum razer_mouse_res *res_list, int count);

/** razer_free_leds - Free a linked list of struct razer_led.
  * This function frees a whole linked list of struct razer_led,
  * as returned by the device methods. Note that you can
  * also free a single struct razer_led with this function, if
  * you assign a NULL pointer to led_list->next before calling this.
  */
void razer_free_leds(struct razer_led *led_list);

/** razer_rescan_mice - Rescan for connected razer mice.
  * Returns a pointer to the linked list of mice, or a NULL pointer
  * in case of an error.
  */
struct razer_mouse * razer_rescan_mice(void);

/** razer_for_each_mouse - Convenience helper for traversing a mouse list
 *
 * @mouse: 'struct razer_mouse' pointer used as a list pointer.
 * @next: 'struct razer_mouse' pointer used as temporary 'next' pointer.
 * @mice_list: Pointer to the base of the linked list.
 *
 * Use razer_for_each_mouse like a normal C 'for' loop.
 */
#define razer_for_each_mouse(mouse, next, mice_list) \
	for (mouse = mice_list, next = (mice_list) ? (mice_list)->next : NULL; \
	     mouse; \
	     mouse = next, next = (mouse) ? (mouse)->next : NULL)

/** enum razer_event - The type of an event.
 */
enum razer_event {
	RAZER_EV_MOUSE_ADD,
	RAZER_EV_MOUSE_REMOVE,
};

/** struct razer_event_data - Context data for an event.
 */
struct razer_event_data {
	union {
		struct razer_mouse *mouse;
	} u;
};

/** razer_event_handler_t - The type of an event handler.
 */
typedef void (*razer_event_handler_t)(enum razer_event event,
				      const struct razer_event_data *data);

/** razer_register_event_handler - Register an event handler.
 */
int razer_register_event_handler(razer_event_handler_t handler);

/** razer_unregister_event_handler - Unregister an event handler.
 */
void razer_unregister_event_handler(razer_event_handler_t handler);

/** razer_load_config - Load a configuration file.
 * If path is NULL, the default config is loaded.
 * If path is an empty string, the current config (if any) will be
 * discarded and no config will be loaded.
 */
int razer_load_config(const char *path);

typedef void (*razer_logfunc_t)(const char *fmt, ...);

/** razer_set_logging - Set log callbacks.
 * Callbacks may be NULL to suppress messages.
 */
void razer_set_logging(razer_logfunc_t info_callback,
		       razer_logfunc_t error_callback,
		       razer_logfunc_t debug_callback);

/** razer_init - LibRazer initialization
  * Call this before any other library function.
  */
int razer_init(int enable_profile_emu);

/** razer_exit - LibRazer cleanup
  * Call this after any operation with the library.
  */
void razer_exit(void);

#endif /* LIB_RAZER_H_ */
