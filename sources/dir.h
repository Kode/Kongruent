#ifndef KONG_DIR_HEADER
#define KONG_DIR_HEADER

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct directory {
	void *handle;
} directory;

typedef struct file {
	bool valid;
	char name[256];
} file;

directory open_dir(const char *dirname);
file      read_next_file(directory *dir);
void      close_dir(directory *dir);

#ifdef __cplusplus
}
#endif

#endif
