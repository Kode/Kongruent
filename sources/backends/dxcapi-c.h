#ifndef DXCAPI_C_HEADER
#define DXCAPI_C_HEADER

#include <initguid.h>

static const CLSID CLSID_DxcCompiler = {0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}};
static const IID   IID_IDxcCompiler3 = {0x228b4687, 0x5a6a, 0x4730, {0x90, 0x0c, 0x97, 0x02, 0xb2, 0x20, 0x3f, 0x54}};
static const IID   IID_IDxcResult    = {0x58346cda, 0xdde7, 0x4497, {0x94, 0x61, 0x6f, 0x87, 0xaf, 0x5e, 0x06, 0x59}};
static const IID   IID_IDxcBlobUtf8  = {0x3da636c9, 0xba71, 0x4024, {0xa3, 0x01, 0x30, 0xcb, 0xf1, 0x25, 0x30, 0x5b}};
static const IID   IID_IDxcBlob      = {0x8ba5fb08, 0x5195, 0x40e2, {0xac, 0x58, 0x0d, 0x98, 0x9c, 0x3a, 0x01, 0x02}};

#ifndef HRESULT
#define HRESULT long
#endif

#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif

typedef enum DXC_OUT_KIND {
	DXC_OUT_NONE        = 0,
	DXC_OUT_OBJECT      = 1,
	DXC_OUT_ERRORS      = 2,
	DXC_OUT_PDB         = 3,
	DXC_OUT_SHADER_HASH = 4,
} DXC_OUT_KIND;

typedef struct IDxcResult   IDxcResult;
typedef struct IDxcBlobWide IDxcBlobWide;

typedef struct IDxcResultVtbl {
	HRESULT(__stdcall *QueryInterface)(struct IDxcResult *This, const IID *riid, void **ppvObject);
	unsigned long(__stdcall *AddRef)(struct IDxcResult *This);
	unsigned long(__stdcall *Release)(struct IDxcResult *This);

	HRESULT(__stdcall *GetStatus)(IDxcResult *This, HRESULT *pStatus);
	HRESULT(__stdcall *GetResult)(IDxcResult *This, void **ppResult);
	HRESULT(__stdcall *GetErrorBuffer)(IDxcResult *This, void **ppErrors);

	int(__stdcall *HasOutput)(IDxcResult *This, DXC_OUT_KIND dxcOutKind);
	HRESULT(__stdcall *GetOutput)(struct IDxcResult *This, DXC_OUT_KIND dxcOutKind, const IID *iid, void **ppResult, IDxcBlobWide **ppOutputName);
} IDxcResultVtbl;

struct IDxcResult {
	const IDxcResultVtbl *lpVtbl;
};

#define DXC_CP_UTF8 65001

typedef struct DxcBuffer {
	const void *Ptr;
	size_t      Size;
	uint32_t    Encoding;
} DxcBuffer;

typedef struct IDxcCompiler3      IDxcCompiler3;
typedef struct IDxcIncludeHandler IDxcIncludeHandler;

typedef struct IDxcCompiler3Vtbl {
	HRESULT(__stdcall *QueryInterface)(IDxcCompiler3 *This, const IID *riid, void **ppvObject);
	unsigned long(__stdcall *AddRef)(IDxcCompiler3 *This);
	unsigned long(__stdcall *Release)(IDxcCompiler3 *This);

	HRESULT(__stdcall *Compile)
	(IDxcCompiler3 *This, const DxcBuffer *pSource, const wchar_t **pArguments, uint32_t argCount, IDxcIncludeHandler *pIncludeHandler, const IID *iid,
	 IDxcResult **ppResult);
} IDxcCompiler3Vtbl;

struct IDxcCompiler3 {
	const IDxcCompiler3Vtbl *lpVtbl;
};

typedef struct IDxcBlob IDxcBlob;

typedef struct DxcBuffer DxcBuffer;

typedef struct IDxcBlobVtbl {
	HRESULT(__stdcall *QueryInterface)(IDxcBlob *This, const IID *riid, void **ppvObject);
	unsigned long(__stdcall *AddRef)(IDxcBlob *This);
	unsigned long(__stdcall *Release)(IDxcBlob *This);

	void *(__stdcall *GetBufferPointer)(IDxcBlob *This);
	unsigned __int64(__stdcall *GetBufferSize)(IDxcBlob *This);
} IDxcBlobVtbl;

struct IDxcBlob {
	const IDxcBlobVtbl *lpVtbl;
};

typedef struct IDxcBlobUtf8 IDxcBlobUtf8;

typedef struct IDxcBlobUtf8Vtbl {
	HRESULT(__stdcall *QueryInterface)(IDxcBlobUtf8 *This, const IID *riid, void **ppvObject);
	unsigned long(__stdcall *AddRef)(IDxcBlobUtf8 *This);
	unsigned long(__stdcall *Release)(IDxcBlobUtf8 *This);

	void *(__stdcall *GetBufferPointer)(IDxcBlobUtf8 *This);
	unsigned __int64(__stdcall *GetBufferSize)(IDxcBlobUtf8 *This);

	HRESULT(__stdcall *GetEncoding)(IDxcBlobUtf8 *This, int *pKnown, uint32_t *pCodePage);

	char *(__stdcall *GetStringPointer)(IDxcBlobUtf8 *This);
	unsigned __int64(__stdcall *GetStringLength)(IDxcBlobUtf8 *This);
} IDxcBlobUtf8Vtbl;

struct IDxcBlobUtf8 {
	const IDxcBlobUtf8Vtbl *lpVtbl;
};

struct IDxcBlobWide {
	int nothing;
};

HRESULT __stdcall DxcCreateInstance(const CLSID *rclsid, const IID *riid, IDxcCompiler3 **ppv);

#endif
