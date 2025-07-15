#include "d3d12.h"

#include "../errors.h"

#ifdef _WIN32

#include "../log.h"

// #include <dxcapi.h>

#include <initguid.h>
#include <stdlib.h>

#endif

#ifdef _WIN32
static const wchar_t *shader_string(shader_stage stage) {
	switch (stage) {
	case SHADER_STAGE_VERTEX:
		return L"vs_6_0";
	case SHADER_STAGE_FRAGMENT:
		return L"ps_6_0";
	case SHADER_STAGE_COMPUTE:
		return L"cs_6_0";
	case SHADER_STAGE_RAY_GENERATION:
		return L"lib_6_3";
	case SHADER_STAGE_AMPLIFICATION:
		return L"as_6_5";
	case SHADER_STAGE_MESH:
		return L"ms_6_5";
	default: {
		debug_context context = {0};
		error(context, "Unsupported shader stage/version combination");
		return L"unsupported";
	}
	}
}
static const CLSID CLSID_DxcCompiler = {0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}};
static const IID   IID_IDxcCompiler3 = {0x228b4687, 0x5a6a, 0x4730, {0x90, 0x0c, 0x97, 0x02, 0xb2, 0x20, 0x3f, 0x54}};
static const IID   IID_IDxcResult    = {0x58346cda, 0xdde7, 0x4497, {0x94, 0x61, 0x6f, 0x87, 0xaf, 0x5e, 0x06, 0x59}};
static const IID   IID_IDxcBlobUtf8  = {0x3da636c9, 0xba71, 0x4024, {0xa3, 0x01, 0x30, 0xcb, 0xf1, 0x25, 0x30, 0x5b}};
static const IID   IID_IDxcBlob      = {0x8ba5fb08, 0x5195, 0x40e2, {0xac, 0x58, 0x0d, 0x98, 0x9c, 0x3a, 0x01, 0x02}};

#ifndef LPCWSTR
#define LPCWSTR const wchar_t *
#endif

#ifndef UINT32
#define UINT32 uint32_t
#endif

#ifndef HRESULT
#define HRESULT long
#endif

typedef unsigned long DWORD;

typedef DWORD ULONG;

typedef int BOOL;

typedef unsigned __int64 ULONG_PTR;

typedef ULONG_PTR SIZE_T;

#define S_OK ((HRESULT)0L)

#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE __stdcall
#endif

typedef struct IDxcCompiler3      IDxcCompiler3;
typedef struct DxcBuffer          DxcBuffer;
typedef struct IDxcIncludeHandler IDxcIncludeHandler;
typedef struct IDxcBlobUtf8       IDxcBlobUtf8;
typedef struct IDxcBlobWide       IDxcBlobWide;

#define DXC_CP_UTF8 65001

typedef enum DXC_OUT_KIND {
	DXC_OUT_NONE        = 0,
	DXC_OUT_OBJECT      = 1,
	DXC_OUT_ERRORS      = 2,
	DXC_OUT_PDB         = 3,
	DXC_OUT_SHADER_HASH = 4,
} DXC_OUT_KIND;

typedef struct IDxcResult IDxcResult;

typedef struct IDxcResultVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(struct IDxcResult *This, const IID *riid, void **ppvObject);
	ULONG(STDMETHODCALLTYPE *AddRef)(struct IDxcResult *This);
	ULONG(STDMETHODCALLTYPE *Release)(struct IDxcResult *This);

	HRESULT(STDMETHODCALLTYPE *GetStatus)(IDxcResult *This, HRESULT *pStatus);
	HRESULT(STDMETHODCALLTYPE *GetResult)(IDxcResult *This, void **ppResult);
	HRESULT(STDMETHODCALLTYPE *GetErrorBuffer)(IDxcResult *This, void **ppErrors);

	BOOL(STDMETHODCALLTYPE *HasOutput)(IDxcResult *This, DXC_OUT_KIND dxcOutKind);
	HRESULT(STDMETHODCALLTYPE *GetOutput)(struct IDxcResult *This, DXC_OUT_KIND dxcOutKind, const IID *iid, void **ppResult, IDxcBlobWide **ppOutputName);
} IDxcResultVtbl;

struct IDxcResult {
	const IDxcResultVtbl *lpVtbl;
};

typedef struct IDxcCompiler3Vtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(IDxcCompiler3 *This, const IID *riid, void **ppvObject);
	ULONG(STDMETHODCALLTYPE *AddRef)(IDxcCompiler3 *This);
	ULONG(STDMETHODCALLTYPE *Release)(IDxcCompiler3 *This);

	HRESULT(STDMETHODCALLTYPE *Compile)(IDxcCompiler3 *This, const DxcBuffer *pSource, LPCWSTR *pArguments, UINT32 argCount,
	                                    IDxcIncludeHandler *pIncludeHandler, const IID *iid, IDxcResult **ppResult);
} IDxcCompiler3Vtbl;

struct IDxcCompiler3 {
	const IDxcCompiler3Vtbl *lpVtbl;
};

typedef struct DxcBuffer {
	const void *Ptr;
	size_t      Size;
	uint32_t    Encoding; // 0 = unknown, 1200 = UTF16, 65001 = UTF8
} DxcBuffer;

typedef struct IDxcBlob IDxcBlob;

typedef struct IDxcBlobVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(IDxcBlob *This, const IID *riid, void **ppvObject);
	ULONG(STDMETHODCALLTYPE *AddRef)(IDxcBlob *This);
	ULONG(STDMETHODCALLTYPE *Release)(IDxcBlob *This);

	void *(STDMETHODCALLTYPE *GetBufferPointer)(IDxcBlob *This);
	SIZE_T(STDMETHODCALLTYPE *GetBufferSize)(IDxcBlob *This);
} IDxcBlobVtbl;

struct IDxcBlob {
	const IDxcBlobVtbl *lpVtbl;
};

typedef struct IDxcBlobUtf8Vtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(IDxcBlobUtf8 *This, const IID *riid, void **ppvObject);
	ULONG(STDMETHODCALLTYPE *AddRef)(IDxcBlobUtf8 *This);
	ULONG(STDMETHODCALLTYPE *Release)(IDxcBlobUtf8 *This);

	void *(STDMETHODCALLTYPE *GetBufferPointer)(IDxcBlobUtf8 *This);
	SIZE_T(STDMETHODCALLTYPE *GetBufferSize)(IDxcBlobUtf8 *This);

	HRESULT(STDMETHODCALLTYPE *GetEncoding)(IDxcBlobUtf8 *This, BOOL *pKnown, UINT32 *pCodePage);

	char *(STDMETHODCALLTYPE *GetStringPointer)(IDxcBlobUtf8 *This);
	SIZE_T(STDMETHODCALLTYPE *GetStringLength)(IDxcBlobUtf8 *This);
} IDxcBlobUtf8Vtbl;

struct IDxcBlobUtf8 {
	const IDxcBlobUtf8Vtbl *lpVtbl;
};

struct IDxcBlobWide {
	int nothing;
	// const IDxcBlobWideVtbl *lpVtbl;
};

HRESULT __stdcall DxcCreateInstance(const CLSID *rclsid, const IID *riid, IDxcCompiler3 **ppv);

#endif

int compile_hlsl_to_d3d12(const char *source, uint8_t **output, size_t *outputlength, shader_stage stage, bool debug) {
#ifdef _WIN32
	IDxcCompiler3 *compiler;
	HRESULT        result = DxcCreateInstance(&CLSID_DxcCompiler, &IID_IDxcCompiler3, &compiler);
	assert(result == S_OK);

	const wchar_t *compiler_args[] = {
	    L"-E", L"main", L"-T", shader_string(stage),
	    // L"-Qstrip_reflect", // strip reflection into a seperate blob
	};

	const wchar_t *debug_compiler_args[] = {
	    L"-E",  L"main", L"-T", shader_string(stage),
	    L"-Zi", // enable debug info
	    // L"-Fd", L"myshader.pdb", // the file name of the pdb.  This must either be supplied or the auto generated file name must be used
	    // L"myshader.hlsl", // optional shader source file name for error reporting and for PIX shader source view
	    // L"-Qstrip_reflect", // strip reflection into a seperate blob
	};

	DxcBuffer source_buffer;
	source_buffer.Ptr      = source;
	source_buffer.Size     = strlen(source);
	source_buffer.Encoding = DXC_CP_UTF8;

	kong_log(LOG_LEVEL_INFO, "IID_IDxcResult: ");
	for (size_t i = 0; i < sizeof(IID); ++i)
		kong_log(LOG_LEVEL_INFO, "%02X ", ((unsigned char *)&IID_IDxcResult)[i]);
	kong_log(LOG_LEVEL_INFO, "");

	IDxcResult *compiler_result = NULL;
	result                      = compiler->lpVtbl->Compile(compiler, &source_buffer, debug ? debug_compiler_args : compiler_args,
                                       debug ? sizeof(debug_compiler_args) / sizeof(wchar_t *) : sizeof(compiler_args) / sizeof(wchar_t *), NULL,
	                                                        &IID_IDxcResult, &compiler_result);
	assert(result == S_OK);

	IDxcBlobUtf8 *errors      = NULL;
	IDxcBlobWide *output_name = NULL;
	result                    = compiler_result->lpVtbl->GetOutput(compiler_result, DXC_OUT_ERRORS, &IID_IDxcBlobUtf8, (void **)&errors, &output_name);
	assert(result == S_OK);

	SIZE_T length  = errors->lpVtbl->GetStringLength(errors);
	char  *pointer = errors->lpVtbl->GetStringPointer(errors);

	if (errors != NULL && errors->lpVtbl->GetStringLength(errors) != 0) {
		kong_log(LOG_LEVEL_INFO, "Warnings and Errors:\n%s", errors->lpVtbl->GetStringPointer(errors));
	}

	HRESULT compilation_result = S_OK;
	result                     = compiler_result->lpVtbl->GetStatus(compiler_result, &compilation_result);
	assert(result == S_OK);

	if (result == S_OK) {
		IDxcBlob     *shader_buffer = NULL;
		IDxcBlobWide *shader_name   = NULL;
		result                      = compiler_result->lpVtbl->GetOutput(compiler_result, DXC_OUT_OBJECT, &IID_IDxcBlob, (void **)&shader_buffer, &shader_name);
		assert(result == S_OK);

		if (shader_buffer == NULL) {
			return 1;
		}
		else {
			*outputlength = shader_buffer->lpVtbl->GetBufferSize(shader_buffer);
			*output       = (uint8_t *)malloc(*outputlength);
			memcpy(*output, shader_buffer->lpVtbl->GetBufferPointer(shader_buffer), shader_buffer->lpVtbl->GetBufferSize(shader_buffer));
		}

#if 0
	            //
	            // save pdb
	            //
	            CComPtr<IDxcBlob> pPDB = nullptr;
	            CComPtr<IDxcBlobUtf16> pPDBName = nullptr;
	            pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), &pPDBName);
	            {
	                FILE* fp = NULL;

	                // note that if you do not specifiy -Fd a pdb name will be automatically generated.  Use this filename to save the pdb so that PIX can find
	       it quickly _wfopen_s(&fp, pPDBName->GetStringPointer(), L"wb"); fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp); fclose(fp);
	            }

	            //
	            // print hash
	            //
	            CComPtr<IDxcBlob> pHash = nullptr;
	            pResults->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHash), nullptr);
	            if (pHash != nullptr)
	            {
	                wprintf(L"Hash: ");
	                DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();
	                for (int i = 0; i < _countof(pHashBuf->HashDigest); i++)
	                    wprintf(L"%x", pHashBuf->HashDigest[i]);
	                wprintf(L"\n");
	            }
#endif

		return 0;
	}
	else {
		return 1;
	}
	return 0;
#else
	return 1;
#endif
}
