#ifndef CONFIG_FILE_H_
#define CONFIG_FILE_H_

struct config_item;
struct config_section;
struct config_file;

struct config_item {
	struct config_section *section;
	char *name;
	char *value;

	struct config_item *next;
};

struct config_section {
	struct config_file *file;
	char *name;

	struct config_section *next;
	struct config_item *items;
};

struct config_file {
	char *path;
	struct config_section *sections;
};

enum {
	CONF_SECT_NOCASE	= (1 << 0), /* Ignore case on section names. */
	CONF_ITEM_NOCASE	= (1 << 1), /* Ignore case on item names. */
	CONF_VALUE_NOCASE	= (1 << 2), /* Ignore case on item values (only for bool). */

	CONF_NOCASE		= CONF_SECT_NOCASE | CONF_ITEM_NOCASE | CONF_VALUE_NOCASE,
};

const char * config_get(struct config_file *f,
			const char *section,
			const char *item,
			const char *_default,
			unsigned int flags);

int config_get_int(struct config_file *f,
		   const char *section,
		   const char *item,
		   int _default,
		   unsigned int flags);

int config_get_bool(struct config_file *f,
		    const char *section,
		    const char *item,
		    int _default,
		    unsigned int flags);

struct config_file * config_file_parse(const char *path);
void config_file_free(struct config_file *f);

#endif /* CONFIG_FILE_H_ */
