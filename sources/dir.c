#include "dir.h"

#include "log.h"

#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32

#include <stdlib.h>
#include <string.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

typedef struct _FILETIME {
	unsigned long dwLowDateTime;
	unsigned long dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _WIN32_FIND_DATAA {
	unsigned long dwFileAttributes;
	FILETIME      ftCreationTime;
	FILETIME      ftLastAccessTime;
	FILETIME      ftLastWriteTime;
	unsigned long nFileSizeHigh;
	unsigned long nFileSizeLow;
	unsigned long dwReserved0;
	unsigned long dwReserved1;
	char          cFileName[MAX_PATH];
	char          cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

__declspec(dllimport) void *__stdcall FindFirstFileA(const char *lpFileName, _Out_ LPWIN32_FIND_DATAA lpFindFileData);

__declspec(dllimport) int __stdcall FindNextFileA(void *hFindFile, _Out_ LPWIN32_FIND_DATAA lpFindFileData);

__declspec(dllimport) int __stdcall FindClose(void *hFindFile);

__declspec(dllimport) unsigned long __stdcall GetLastError(void);

#define INVALID_HANDLE_VALUE ((void *)(__int64)-1)

directory open_dir(const char *dirname) {
	char pattern[1024];
	strcpy(pattern, dirname);
	strcat(pattern, "\\*");

	WIN32_FIND_DATAA data;
	directory        dir;
	dir.handle = FindFirstFileA(pattern, &data);
	if (dir.handle == INVALID_HANDLE_VALUE) {
		kong_log(LOG_LEVEL_ERROR, "FindFirstFile failed (%d)\n", GetLastError());
		exit(1);
	}
	FindNextFileA(dir.handle, &data);
	return dir;
}

file read_next_file(directory *dir) {
	WIN32_FIND_DATAA data;
	file             file;
	file.valid = FindNextFileA(dir->handle, &data) != 0;
	if (file.valid) {
		strcpy(file.name, data.cFileName);
	}
	else {
		file.name[0] = 0;
	}
	return file;
}

void close_dir(directory *dir) {
	FindClose(dir->handle);
}

#else

#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

directory open_dir(const char *dirname) {
	directory dir;
	dir.handle = opendir(dirname);
	if (dir.handle == NULL) {
		kong_log(LOG_LEVEL_ERROR, "Failed to open directory: %s", dirname);
		exit(1);
	}
	return dir;
}

file read_next_file(directory *dir) {
	struct dirent *entry = readdir(dir->handle);

	while (entry != NULL && (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
		entry = readdir(dir->handle);
	}

	file f;
	f.valid = entry != NULL;

	if (f.valid) {
		strcpy(f.name, entry->d_name);
	}

	return f;
}

void close_dir(directory *dir) {}

#endif
