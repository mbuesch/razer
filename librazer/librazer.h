/*
 *   Razer device access library
 *
 *   Copyright (C) 2007 Michael Buesch <mb@bu3sch.de>
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

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif


enum razer_led_state {
	RAZER_LED_OFF,
	RAZER_LED_ON,
	RAZER_LED_UNKNOWN,
};

struct razer_mouse;

struct razer_led {
	const char *name;
	int id;
	enum razer_led_state state;

	int (*toggle_state)(struct razer_led *led,
			    enum razer_led_state new_state);

	union {
		struct razer_mouse *mouse;
	} u;
	struct razer_led *next;
};

/** enum razer_mouse_freq - Mouse scan frequency
 */
enum razer_mouse_freq {
	RAZER_MOUSE_FREQ_UNKNOWN	= 0,
	RAZER_MOUSE_FREQ_125HZ		= 125,
	RAZER_MOUSE_FREQ_500HZ		= 500,
	RAZER_MOUSE_FREQ_1000HZ		= 1000,
};

/** enum razer_mouse_res - Mouse scan resolutions
  */
enum razer_mouse_res {
	RAZER_MOUSE_RES_UNKNOWN		= 0,
	RAZER_MOUSE_RES_400DPI		= 400,
	RAZER_MOUSE_RES_450DPI		= 450,
	RAZER_MOUSE_RES_900DPI		= 900,
	RAZER_MOUSE_RES_1600DPI		= 1600,
	RAZER_MOUSE_RES_1800DPI		= 1800,
};

/** enum razer_mouse_type
  * @RAZER_MOUSETYPE_DEATHADDER: A "DeathAdder" mouse
  * @RAZER_MOUSETYPE_KRAIT: A "Krait" mouse
  * @RAZER_MOUSETYPE_LACHESIS: A "Lachesis" mouse
  */
enum razer_mouse_type {
	RAZER_MOUSETYPE_DEATHADDER,
	RAZER_MOUSETYPE_KRAIT,
	RAZER_MOUSETYPE_LACHESIS,
};

/** struct razer_mouse - Representation of a mouse device
  *
  * @next: Linked list to the next mouse.
  *
  * @type: The mouse type
  *
  * @claim: Claim and open the backend device (USB).
  * 	As long as the device is claimed, it is not operable by the user!
  *
  * @release: Release a claimed backend device.
  *
  * @get_leds: Get a linked list of available LEDs.
  * 	Returns the number of LEDs or a negative error code.
  * 	leds_list points to the first LED in the list.
  * 	The caller is responsible to free every item in leds_list.
  *
  * @supported_freqs: Get an array of supported scan frequencies.
  * 	Returns the array size or a negative error code.
  * 	freq_ptr points to the array.
  * 	The caller is responsible to free freq_ptr.
  *
  * @get_freq: Get the currently used scan frequency.
  *
  * @set_freq: Change the mouse scan frequency.
  *
  * @supported_resolutions: Get an array of supported scan resolutions.
  * 	Returns the array size or a negative error code.
  * 	res_ptr points to the array.
  * 	The caller is responsible to free res_ptr.
  *
  * @get_resolution: Get the currently used scan resolution.
  *
  * @set_resolution: Change the mouse scan resolution.
  */
struct razer_mouse {
	struct razer_mouse *next;

	enum razer_mouse_type type;

	int (*claim)(struct razer_mouse *m);
	void (*release)(struct razer_mouse *m);

	int (*get_leds)(struct razer_mouse *m,
			struct razer_led **leds_list);

	int (*supported_freqs)(struct razer_mouse *m,
			       enum razer_mouse_freq **freq_ptr);
	enum razer_mouse_freq (*get_freq)(struct razer_mouse *m);
	int (*set_freq)(struct razer_mouse *m, enum razer_mouse_freq freq);

	int (*supported_resolutions)(struct razer_mouse *m,
				     enum razer_mouse_res **res_ptr);
	enum razer_mouse_res (*get_resolution)(struct razer_mouse *m);
	int (*set_resolution)(struct razer_mouse *m, enum razer_mouse_res res);

	void *internal; /* Do not touch this pointer. */
};

//XXX docs
void razer_free_freq_list(enum razer_mouse_freq *freq_list, int count);
void razer_free_resolution_list(enum razer_mouse_res *res_list, int count);

void razer_free_leds(struct razer_led *led_list);
void razer_free_led(struct razer_led *led);

/** razer_scan_mice - Scan the system for connected razer mice.
  * Returns the number of detected mice or a negative error code.
  * The function will put a linked list of mice into mice_list.
  * The caller is responsible to free each mouse in the list
  * after use.
  */
int razer_scan_mice(struct razer_mouse **mice_list);

/* razer_free_mice - Free the list of mice from razer_scan_mice
 */
void razer_free_mice(struct razer_mouse *mouse_list);
/* razer_free_mouse - Free a single mouse
 */
void razer_free_mouse(struct razer_mouse *mouse);

/** razer_init - LibRazer initialization
  * Call this before any other library function.
  */
int razer_init(void);

/** razer_exit - LibRazer cleanup
  * Call this after any operation with the library.
  */
void razer_exit(void);


#if defined(c_plusplus) || defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* LIB_RAZER_H_ */
