#ifndef RAZER_PRIVATE_H_
#define RAZER_PRIVATE_H_

#include "librazer.h"

#include <usb.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <stdint.h>
#include <stdlib.h>



#ifdef DEBUG
# define dprintf(...)		printf("[librazer debug]: " __VA_ARGS__)
#else
# define dprintf		noprintf
#endif
static inline int noprintf(const char *t, ...) { return 0; }

#define ARRAY_SIZE(array)	(sizeof(array) / sizeof((array)[0]))


#define for_each_usbbus(bus, buslist) \
	for (bus = buslist; bus; bus = bus->next)
#define for_each_usbdev(dev, devlist) \
	for (dev = devlist; dev; dev = dev->next)


typedef _Bool bool;
#undef min
#undef max
#undef offsetof
#define offsetof(type, member)	((size_t)&((type *)0)->member)
#define min(x, y)		({ __typeof__(x) __x = (x); \
				   __typeof__(y) __y = (y); \
				   __x < __y ? __x : __y; })
#define max(x, y)		({ __typeof__(x) __x = (x); \
				   __typeof__(y) __y = (y); \
				   __x > __y ? __x : __y; })

struct razer_usb_context {
	/* Device pointer. */
	struct usb_device *dev;
	/* The handle for all operations. */
	struct usb_dev_handle *h;
	/* The interface number we use. */
	int interf;
	/* Did we detach the kernel driver? */
	bool kdrv_detached;
};

int razer_generic_usb_claim(struct razer_usb_context *ctx);
void razer_generic_usb_release(struct razer_usb_context *ctx);

struct razer_usb_reconnect_guard {
	struct razer_usb_context *ctx;
	struct usb_device_descriptor old_desc;
	char old_dirname[PATH_MAX + 1];
	char old_filename[PATH_MAX + 1];
};

int razer_usb_reconnect_guard_init(struct razer_usb_reconnect_guard *guard,
				   struct razer_usb_context *ctx);
int razer_usb_reconnect_guard_wait(struct razer_usb_reconnect_guard *guard, bool hub_reset);

int razer_usb_force_reinit(struct razer_usb_context *ctx);

void razer_msleep(unsigned int msecs);

#define BUSTYPESTR_USB		"USB"
#define DEVTYPESTR_MOUSE	"Mouse"
static inline void razer_create_idstr(char *buf,
				      const char *bustype,
				      const char *busposition,
				      const char *devtype,
				      const char *devname,
				      const char *devid)
{
	snprintf(buf, RAZER_IDSTR_MAX_SIZE, "%s:%s:%s-%s:%s",
		 devtype, devname, bustype, busposition, devid);
}


typedef uint16_t	be16_t;
typedef uint32_t	be32_t;
typedef uint16_t	le16_t;
typedef uint32_t	le32_t;

#ifdef BIG_ENDIAN_HOST
# define RAZER_HOST_BE	1
#else
# define RAZER_HOST_BE	0
#endif

static inline uint16_t cond_swap16(uint16_t v, bool to_from_bigendian)
{
	if ((unsigned int)to_from_bigendian ^ RAZER_HOST_BE)
		return bswap_16(v);
	return v;
}

static inline uint32_t cond_swap32(uint32_t v, bool to_from_bigendian)
{
	if ((unsigned int)to_from_bigendian ^ RAZER_HOST_BE)
		return bswap_32(v);
	return v;
}

static inline be16_t cpu_to_be16(uint16_t v) { return (be16_t)cond_swap16(v, 1);   }
static inline uint16_t be16_to_cpu(be16_t v) { return cond_swap16((uint16_t)v, 1); }
static inline be32_t cpu_to_be32(uint32_t v) { return (be32_t)cond_swap32(v, 1);   }
static inline uint32_t be32_to_cpu(be32_t v) { return cond_swap32((uint32_t)v, 1); }
static inline le16_t cpu_to_le16(uint16_t v) { return (le16_t)cond_swap16(v, 0);   }
static inline uint16_t le16_to_cpu(le16_t v) { return cond_swap16((uint16_t)v, 0); }
static inline le32_t cpu_to_le32(uint32_t v) { return (le32_t)cond_swap32(v, 0);   }
static inline uint32_t le32_to_cpu(le32_t v) { return cond_swap32((uint32_t)v, 0); }


le16_t razer_xor16_checksum(const void *buffer, size_t size);

#ifdef DEBUG
void razer_dump(const char *prefix, const void *buf, size_t size);
#else
static inline void razer_dump(const char *p, const void *b, size_t s) { }
#endif

#define BUILD_BUG_ON(x)		((void)sizeof(char[1 - 2 * !!(x)]))

static inline void * zalloc(size_t size)
{
	return calloc(1, size);
}

#endif /* RAZER_PRIVATE_H_ */
