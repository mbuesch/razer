#ifndef LIBRAZER_BUTTONMAPPING_H_
#define LIBRAZER_BUTTONMAPPING_H_

#include "razer_private.h"


/* enum razer_button_function_id - Logical function IDs */
enum razer_button_function_id {
	RAZER_BUTFUNC_LEFT		= 0x01, /* Left button */
	RAZER_BUTFUNC_RIGHT		= 0x02, /* Right button */
	RAZER_BUTFUNC_MIDDLE		= 0x03, /* Middle button */
	RAZER_BUTFUNC_DBLCLICK		= 0x04, /* Left button double click */
	RAZER_BUTFUNC_ADVANCED		= 0x05,	/* Advanced function */
	RAZER_BUTFUNC_MACRO		= 0x06, /* Macro function */
	RAZER_BUTFUNC_PROFDOWN		= 0x0A, /* Profile down */
	RAZER_BUTFUNC_PROFUP		= 0x0B, /* Profile up */
	RAZER_BUTFUNC_DPIUP		= 0x0C, /* DPI down */
	RAZER_BUTFUNC_DPIDOWN		= 0x0D, /* DPI down */
	RAZER_BUTFUNC_DPI1		= 0x0E, /* Select first DPI mapping */
	RAZER_BUTFUNC_DPI2		= 0x0F, /* Select second DPI mapping */
	RAZER_BUTFUNC_DPI3		= 0x10, /* Select third DPI mapping */
	RAZER_BUTFUNC_DPI4		= 0x11, /* Select fourth DPI mapping */
	RAZER_BUTFUNC_DPI5		= 0x12, /* Select fifth DPI mapping */
	RAZER_BUTFUNC_WIN5		= 0x1A, /* Windows button 5 */
	RAZER_BUTFUNC_WIN4		= 0x1B, /* Windows button 4 */
	RAZER_BUTFUNC_SCROLLUP		= 0x30, /* Scroll wheel up */
	RAZER_BUTFUNC_SCROLLDWN		= 0x31, /* Scroll wheel down */
};

/* Define a struct razer_button_function element */
#define DEFINE_RAZER_BUTFUNC(_id, _name) \
	{ .id = RAZER_BUTFUNC_##_id,  .name = _name, }

#define BUTTONFUNC_LEFT		DEFINE_RAZER_BUTFUNC(LEFT,	"Leftclick")
#define BUTTONFUNC_RIGHT	DEFINE_RAZER_BUTFUNC(RIGHT,	"Rightclick")
#define BUTTONFUNC_MIDDLE	DEFINE_RAZER_BUTFUNC(MIDDLE,	"Middleclick")
#define BUTTONFUNC_DBLCLICK	DEFINE_RAZER_BUTFUNC(DBLCLICK,	"Doubleclick")
#define BUTTONFUNC_ADVANCED	DEFINE_RAZER_BUTFUNC(ADVANCED,	"Advanced")
#define BUTTONFUNC_MACRO	DEFINE_RAZER_BUTFUNC(MACRO,	"Macro")
#define BUTTONFUNC_PROFDOWN	DEFINE_RAZER_BUTFUNC(PROFDOWN,	"Profile switch down")
#define BUTTONFUNC_PROFUP	DEFINE_RAZER_BUTFUNC(PROFUP,	"Profile switch up")
#define BUTTONFUNC_DPIUP	DEFINE_RAZER_BUTFUNC(DPIUP,	"DPI mapping up")
#define BUTTONFUNC_DPIDOWN	DEFINE_RAZER_BUTFUNC(DPIDOWN,	"DPI mapping down")
#define BUTTONFUNC_DPI1		DEFINE_RAZER_BUTFUNC(DPI1,	"1st DPI mapping")
#define BUTTONFUNC_DPI2		DEFINE_RAZER_BUTFUNC(DPI2,	"2nd DPI mapping")
#define BUTTONFUNC_DPI3		DEFINE_RAZER_BUTFUNC(DPI3,	"3rd DPI mapping")
#define BUTTONFUNC_DPI4		DEFINE_RAZER_BUTFUNC(DPI4,	"4th DPI mapping")
#define BUTTONFUNC_DPI5		DEFINE_RAZER_BUTFUNC(DPI5,	"5th DPI mapping")
#define BUTTONFUNC_WIN5		DEFINE_RAZER_BUTFUNC(WIN5,	"Windows button 5")
#define BUTTONFUNC_WIN4		DEFINE_RAZER_BUTFUNC(WIN4,	"Windows button 4")
#define BUTTONFUNC_SCROLLUP	DEFINE_RAZER_BUTFUNC(SCROLLUP,	"Scroll wheel up")
#define BUTTONFUNC_SCROLLDWN	DEFINE_RAZER_BUTFUNC(SCROLLDWN,	"Scroll wheel down")


/* struct razer_buttonmapping - physical-logical mapping for one button.
 * This is the wire-protocol data structure. */
struct razer_buttonmapping {
	uint8_t physical;
	uint8_t logical;
};

/** razer_create_buttonmap - Create an on-wire button map. */
int razer_create_buttonmap(void *buffer, size_t bufsize,
			   struct razer_buttonmapping *mappings, size_t nr_mappings,
			   unsigned int struct_spacing);

/** razer_parse_buttonmap - Parse an on-wire button map. */
int razer_parse_buttonmap(void *rawdata, size_t rawsize,
			  struct razer_buttonmapping *mappings, size_t nr_mappings,
			  unsigned int struct_spacing);

/** razer_get_buttonfunction_by_id - find a function in a list, by ID */
struct razer_button_function * razer_get_buttonfunction_by_id(
		struct razer_button_function *functions, size_t nr_functions,
		uint8_t logical_id);

/** razer_get_buttonfunction_by_button - find a function in a list, by button */
struct razer_button_function * razer_get_buttonfunction_by_button(
		struct razer_buttonmapping *mappings, size_t nr_mappings,
		struct razer_button_function *functions, size_t nr_functions,
		const struct razer_button *button);

/** razer_get_buttonmapping_by_physid - find a button mapping by physical ID */
struct razer_buttonmapping * razer_get_buttonmapping_by_physid(
		struct razer_buttonmapping *mappings, size_t nr_mappings,
		uint8_t physical_id);

#endif /* LIBRAZER_BUTTONMAPPING_H_ */
