/*
 *   Razer lowlevel device access library.
 *   Applications do NOT want to use this.
 *   Applications should use pyrazer or librazerd instead.
 *
 *   Copyright (C) 2007-2011 Michael Buesch <m@bues.ch>
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
#include <stdint.h>
#include <stddef.h>


#define RAZER_IDSTR_MAX_SIZE	128
#define RAZER_LEDNAME_MAX_SIZE	64
#define RAZER_DEFAULT_CONFIG	"/etc/razer.conf"

/* Opaque internal data structures */
struct razer_usb_context;
struct razer_mouse_base_ops;
struct razer_mouse_profile_emu;

struct razer_mouse;


/** razer_utf16_t - UTF-16 type */
typedef uint16_t razer_utf16_t;

/** razer_ascii_to_utf16 - Convert ASCII to UTF16.
 * @dest: Destination buffer.
 * @dest_max_chars: Maximum number of characters in dest.
 * @src: NUL terminated ASCII source buffer.
 */
void razer_ascii_to_utf16(razer_utf16_t *dest, size_t dest_max_chars,
			  const char *src);

/** razer_utf16_cpy - Copy an UTF16 string.
 * @dest: Destination buffer.
 * @src: Source buffer.
 * @max_chars: Maximum number of characters to copy.
 */
int razer_utf16_cpy(razer_utf16_t *dest, const razer_utf16_t *src,
		    size_t max_chars);

/** razer_utf16_strlen - Return the length of an UTF16 string.
 * @str: An UTF16 string.
 */
size_t razer_utf16_strlen(const razer_utf16_t *str);

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

/** struct razer_rgb_color - An RGB color
 * @r: Red value.
 * @g: Green value.
 * @b: Blue value.
 * @valid: 1 if this color is valid. 0 otherise.
 */
struct razer_rgb_color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t valid;
};

/** struct razer_led - A LED on a razer device.
  *
  * @next: The next LED device in the linked list.
  *
  * @name: The human readable name string for the LED.
  * @id: A unique ID cookie
  * @state: The state of the LED (on, off, unknown)
  * @color: The color of the LED.
  *
  * @toggle_state: Toggle the state. Note that a new_state of
  * 	RAZER_LED_UNKNOWN result is an error.
  *
  * @change_color: Change the color of the LED.
  *	May be NULL, if the color cannot be changed.
  *
  * @u: This union contains a pointer to the parent device.
  */
struct razer_led {
	struct razer_led *next;

	const char *name;
	unsigned int id;
	enum razer_led_state state;
	struct razer_rgb_color color;

	int (*toggle_state)(struct razer_led *led,
			    enum razer_led_state new_state);

	int (*change_color)(struct razer_led *led,
			    const struct razer_rgb_color *new_color);

	union {
		struct razer_mouse *mouse;
		struct razer_mouse_profile *mouse_prof;
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
	RAZER_MOUSE_RES_100DPI		= 100,
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
	RAZER_MOUSE_RES_5600DPI		= 5600,
	RAZER_MOUSE_RES_6000DPI		= 6000,
	RAZER_MOUSE_RES_6400DPI		= 6400,
	RAZER_MOUSE_RES_7000DPI		= 7000,
	RAZER_MOUSE_RES_7600DPI		= 7600,
	RAZER_MOUSE_RES_8200DPI		= 8200,
};

/** enum razer_mouse_type
  * @RAZER_MOUSETYPE_DEATHADDER: A "DeathAdder" mouse
  * @RAZER_MOUSETYPE_KRAIT: A "Krait" mouse
  * @RAZER_MOUSETYPE_LACHESIS: A "Lachesis" mouse
  * @RAZER_MOUSETYPE_LACHESIS: A "Copperhead" mouse
  * @RAZER_MOUSETYPE_NAGA: A "Naga" mouse
  * @RAZER_MOUSETYPE_BOOMSLANGCE: A "Boomslang Collector's Edition" mouse
  * @RAZER_MOUSETYPE_IMPERATOR: An "Imperator" mouse
  * @RAZER_MOUSETYPE_TAIPAN: A "Taipan" mouse
  */

enum razer_mouse_type {
	RAZER_MOUSETYPE_DEATHADDER,
	RAZER_MOUSETYPE_KRAIT,
	RAZER_MOUSETYPE_LACHESIS,
	RAZER_MOUSETYPE_COPPERHEAD,
	RAZER_MOUSETYPE_NAGA,
	RAZER_MOUSETYPE_BOOMSLANGCE,
	RAZER_MOUSETYPE_IMPERATOR,
	RAZER_MOUSETYPE_TAIPAN,
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

/** razer_id_mask_t - ID mask type */
typedef uint64_t razer_id_mask_t;

/** razer_id_mask_set - Set a bit in an ID mask.
 *
 * @mask: Pointer to the mask.
 *
 * @nr: The ID number.
 */
static inline void razer_id_mask_set(razer_id_mask_t *mask, unsigned int id)
{
	*mask |= ((razer_id_mask_t)1ull << id);
}

/** razer_id_mask_clear - Clear a bit in an ID mask.
 *
 * @mask: Pointer to the mask.
 *
 * @nr: The ID number.
 */
static inline void razer_id_mask_clear(razer_id_mask_t *mask, unsigned int id)
{
	*mask &= ~((razer_id_mask_t)1ull << id);
}

/** razer_id_mask_zero - Initialize an ID mask to "all unset".
 *
 * @mask: Pointer to the mask.
 */
static inline void razer_id_mask_zero(razer_id_mask_t *mask)
{
	*mask = (razer_id_mask_t)0ull;
}

/** enum razer_dimension - Dimension IDs
 * @RAZER_DIM_X: X dimension
 * @RAZER_DIM_Y: Y dimension
 * @RAZER_DIM_Z: Z dimension
 * @RAZER_NR_DIMS: 3 dimensions ought to be enough in this universe.
 * @RAZER_DIM_0: First dimension. X alias.
 * @RAZER_DIM_1: Second dimension. Y alias.
 * @RAZER_DIM_2: Third dimension. Z alias.
 */
enum razer_dimension {
	RAZER_DIM_X,
	RAZER_DIM_Y,
	RAZER_DIM_Z,

	RAZER_NR_DIMS,

	RAZER_DIM_0	= RAZER_DIM_X,
	RAZER_DIM_1	= RAZER_DIM_Y,
	RAZER_DIM_2	= RAZER_DIM_Z,
};

/** struct razer_mouse_dpimapping - Mouse scan resolution mapping.
 *
 * @nr: The ID number.
 *
 * @res: The resolution values. One per dimension.
 *
 * @dimension_mask: Mask of used dimensions.
 *
 * @profile_mask: A bitmask of which profile this dpimapping is valid for.
 *	A value of 0 indicates "any profile".
 *	If bit0 is set, this means profile 0.
 *	If bit1 is set, this means profile 1. etc...
 *
 * @change: Change this mapping to another resolution value.
 *	May be NULL, if the mapping cannot be changed.
 */
struct razer_mouse_dpimapping {
	unsigned int nr;
	enum razer_mouse_res res[RAZER_NR_DIMS];
	unsigned int dimension_mask;
	razer_id_mask_t profile_mask;

	int (*change)(struct razer_mouse_dpimapping *d,
		      enum razer_dimension dim,
		      enum razer_mouse_res res);

	struct razer_mouse *mouse;
};

/** struct razer_mouse_profile - A mouse profile
 *
 * @nr: The profile ID.
 *
 * @get_name: Get the profile name.
 *	May be NULL.
 *
 * @set_name: Set the profile name.
 *	May be NULL.
 *
 * @get_leds: Get a linked list of per-profile LEDs.
 * 	Returns the number of LEDs or a negative error code.
 * 	leds_list points to the first LED in the list.
 * 	The caller is responsible to free every item in leds_list.
 *	May be NULL.
 *
 * @get_freq: Get the currently used scan frequency.
 *	May be NULL, if the scan frequency is not managed per profile.
 *
 * @set_freq: Change the mouse scan frequency.
 *	May be NULL, if the scan frequency is not managed per profile.
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

	const razer_utf16_t * (*get_name)(struct razer_mouse_profile *p);
	int (*set_name)(struct razer_mouse_profile *p,
			const razer_utf16_t *new_name);

	int (*get_leds)(struct razer_mouse_profile *p,
			struct razer_led **leds_list);

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
 *	sets this to flag on detection. So the highlevel code
 *	using the library can clear this flag to keep track of
 *	devices it already knows about.
 *
 * @RAZER_MOUSEFLG_PROFEMU: Profiles are emulated in software. The device
 *	does only support one profile in hardware.
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
  *	Implicitely commits the config, if the last claim is released.
  *	Returns 0 on success or an error code.
  *	An error is a commit error. The mouse is always released properly.
  *
  * @commit: Commit the current settings.
  *	This usually doesn't have to be called explicitely.
  *	May be NULL.
  *
  * @get_fw_version: Read the firmware version from the device.
  *     Returns the firmware version or a negative error code.
  *
  * @flash_firmware: Upload a firmware image to the device and
  *     flash it to the PROM. &magic_number is &RAZER_FW_FLASH_MAGIC.
  *     The magic is used to project against accidental calls.
  *
  * @global_get_leds: Get a linked list of globally managed LEDs.
  * 	Returns the number of LEDs or a negative error code.
  * 	leds_list points to the first LED in the list.
  * 	The caller is responsible to free every item in leds_list.
  *	May be NULL.
  *
  * @global_get_freq: Get the current globally used scan frequency.
  *	May be NULL, if the scan frequency is not managed globally.
  *
  * @global_set_freq: Change the global mouse scan frequency.
  *	May be NULL, if the scan frequency is not managed globally.
  *
  * @nr_profiles: The number of profiles supported by this device.
  *	Defaults to 1.
  *
  * @get_profiles: Returns an array of supported profiles.
  *	Array length is nr_profiles.
  *
  * @get_active_profile: Returns the currently active profile.
  *	May be NULL, if nr_profiles is 1.
  *
  * @set_active_profile: Selects the active profile.
  *	May be NULL, if nr_profiles is 1.
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
	int (*release)(struct razer_mouse *m);

	int (*commit)(struct razer_mouse *m, int force);

	int (*get_fw_version)(struct razer_mouse *m);

	int (*flash_firmware)(struct razer_mouse *m,
			      const char *data, size_t len,
			      unsigned int magic_number);

	int (*global_get_leds)(struct razer_mouse *m,
			       struct razer_led **leds_list);

	enum razer_mouse_freq (*global_get_freq)(struct razer_mouse *m);
	int (*global_set_freq)(struct razer_mouse *m, enum razer_mouse_freq freq);

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

/** razer_msleep - Delay.
 * msecs: Number of milliseconds to delay.
 */
void razer_msleep(unsigned int msecs);

/** razer_strlcpy - Copy a string into a sized buffer.
 * @dst: Destination buffer.
 * @src: Source string.
 * @dst_size: Destination buffer size.
 */
void razer_strlcpy(char *dst, const char *src, size_t dst_size);

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

/** razer_reconfig_mice - Reconfigure all detected razer mice.
  * Returns 0 on success or an error code.
  */
int razer_reconfig_mice(void);

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
