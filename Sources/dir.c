#include "dir.h"

#include "log.h"

#include <stdio.h>
#include <stddef.h>

#ifdef _WIN32

#include <Windows.h>

directory open_dir(const char *dirname) {
	char pattern[1024];
	strcpy(pattern, dirname);
	strcat(pattern, "\\*");

	WIN32_FIND_DATAA data;
	directory dir;
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
	file file;
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

directory open_dir(const char *dirname) {
	directory dir;
	dir.handle = NULL;
	return dir;
}

file read_next_file(directory *dir) {
	File file;
	file.valid = false;
	return file;
}

void close_dir(directory *dir) {

}

#endif
