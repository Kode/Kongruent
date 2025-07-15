#include "util.h"

#include "../array.h"
#include "../errors.h"

#include <stdlib.h>
#include <string.h>

void indent(char *code, size_t *offset, int indentation) {
	indentation = indentation < 15 ? indentation : 15;
	char str[16];
	memset(str, '\t', sizeof(str));
	str[indentation] = 0;
	*offset += sprintf(&code[*offset], "%s", str);
}

uint32_t base_type_size(type_id type) {
	if (type == float_id) {
		return 4;
	}
	if (type == float2_id) {
		return 4 * 2;
	}
	if (type == float3_id) {
		return 4 * 3;
	}
	if (type == float4_id) {
		return 4 * 4;
	}
	if (type == float4x4_id) {
		return 4 * 4 * 4;
	}
	if (type == float3x3_id) {
		return 3 * 4 * 4;
	}

	if (type == uint_id) {
		return 4;
	}
	if (type == uint2_id) {
		return 4 * 2;
	}
	if (type == uint3_id) {
		return 4 * 3;
	}
	if (type == uint4_id) {
		return 4 * 4;
	}

	if (type == int_id) {
		return 4;
	}
	if (type == int2_id) {
		return 4 * 2;
	}
	if (type == int3_id) {
		return 4 * 3;
	}
	if (type == int4_id) {
		return 4 * 4;
	}

	debug_context context = {0};
	error(context, "Unknown type %s for structure", get_name(get_type(type)->name));
	return 1;
}

uint32_t struct_size(type_id id) {
	uint32_t size = 0;
	type    *t    = get_type(id);
	for (size_t member_index = 0; member_index < t->members.size; ++member_index) {
		size += base_type_size(t->members.m[member_index].type.type);
	}
	return size;
}

#ifdef _WIN32

typedef struct _SECURITY_ATTRIBUTES {
	unsigned long nLength;
	void         *lpSecurityDescriptor;
	int           bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _STARTUPINFOA {
	unsigned long  cb;
	char          *lpReserved;
	char          *lpDesktop;
	char          *lpTitle;
	unsigned long  dwX;
	unsigned long  dwY;
	unsigned long  dwXSize;
	unsigned long  dwYSize;
	unsigned long  dwXCountChars;
	unsigned long  dwYCountChars;
	unsigned long  dwFillAttribute;
	unsigned long  dwFlags;
	unsigned short wShowWindow;
	unsigned short cbReserved2;
	unsigned char *lpReserved2;
	void          *hStdInput;
	void          *hStdOutput;
	void          *hStdError;
} STARTUPINFOA, *LPSTARTUPINFOA;

typedef struct _PROCESS_INFORMATION {
	void         *hProcess;
	void         *hThread;
	unsigned long dwProcessId;
	unsigned long dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;

__declspec(dllimport) int __stdcall CreateProcessA(const char *lpApplicationName, char *lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                                   LPSECURITY_ATTRIBUTES lpThreadAttributes, int bInheritHandles, unsigned long dwCreationFlags,
                                                   void *lpEnvironment, const char *lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo,
                                                   LPPROCESS_INFORMATION lpProcessInformation);

__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *hHandle, unsigned long dwMilliseconds);

__declspec(dllimport) int __stdcall GetExitCodeProcess(void *hProcess, unsigned long *lpExitCode);

__declspec(dllimport) int __stdcall CloseHandle(void *hObject);

#ifndef FALSE
#define FALSE 0
#endif

#ifndef CREATE_DEFAULT_ERROR_MODE
#define CREATE_DEFAULT_ERROR_MODE 0x04000000
#endif

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

#endif

bool execute_sync(const char *command, uint32_t *exit_code) {
#if defined(_WIN32)
	STARTUPINFOA startup_info = {0};
	startup_info.cb           = sizeof(startup_info);

	PROCESS_INFORMATION process_info = {0};

	int success = CreateProcessA(NULL, (char *)command, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &startup_info, &process_info);

	WaitForSingleObject(process_info.hProcess, INFINITE);

	GetExitCodeProcess(process_info.hProcess, exit_code);

	CloseHandle(process_info.hProcess);
	CloseHandle(process_info.hThread);

	return success != FALSE;
#elif defined(__APPLE__)
	int status = system(command);

	if (status < 0) {
		return false;
	}

	*exit_code = (uint32_t)status;

	return true;
#else
	int status = system(command);

	if (status < 0) {
		return false;
	}

	if ((status >> 8) == 0x7f) {
		return false;
	}

	*exit_code = (uint32_t)(status >> 8);

	return true;
#endif
}
