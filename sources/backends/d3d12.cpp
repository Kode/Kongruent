#include "d3d12.h"

#include "../errors.h"

#ifdef _WIN32

#include "../log.h"

#ifdef noreturn
#undef noreturn
#endif

#include <atlbase.h>
#include <dxcapi.h>

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

// static const CLSID CLSID_DxcCompiler = {0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xb6, 0x9c, 0x64, 0x06, 0x8f, 0x88, 0x14}};
static const IID IID_IDxcCompiler3 = {0x228b4687, 0x5a6a, 0x4730, {0x90, 0x0c, 0x97, 0x02, 0xb2, 0x20, 0x3f, 0x54}};
static const IID IID_IDxcResult    = {0x58346cda, 0xdde7, 0x4497, {0x94, 0x61, 0x6f, 0x87, 0xaf, 0x5e, 0x06, 0x59}};
static const IID IID_IDxcBlobUtf8  = {0x3da636c9, 0xba71, 0x4024, {0xa3, 0x01, 0x30, 0xcb, 0xf1, 0x25, 0x30, 0x5b}};
static const IID IID_IDxcBlob      = {0x8ba5fb08, 0x5195, 0x40e2, {0xac, 0x58, 0x0d, 0x98, 0x9c, 0x3a, 0x01, 0x02}};
#endif

int compile_hlsl_to_d3d12(const char *source, uint8_t **output, size_t *outputlength, shader_stage stage, bool debug) {
#ifdef _WIN32
	IDxcCompiler3 *compiler;
	DxcCreateInstance(CLSID_DxcCompiler, IID_IDxcCompiler3, (void **)&compiler);

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

	IDxcResult *compiler_result;
	compiler->Compile(&source_buffer, debug ? debug_compiler_args : compiler_args,
	                  debug ? sizeof(debug_compiler_args) / sizeof(wchar_t *) : sizeof(compiler_args) / sizeof(wchar_t *), NULL, IID_IDxcResult,
	                  (void **)&compiler_result);

	IDxcBlobUtf8 *errors      = NULL;
	IDxcBlobWide *output_name = NULL;
	compiler_result->GetOutput(DXC_OUT_ERRORS, IID_IDxcBlobUtf8, (void **)&errors, &output_name);

	if (errors != NULL && errors->GetStringLength() != 0) {
		kong_log(LOG_LEVEL_INFO, "Warnings and Errors:\n%s", errors->GetStringPointer());
	}

	HRESULT result;
	compiler_result->GetStatus(&result);

	if (result == S_OK) {
		IDxcBlob      *shader_buffer = NULL;
		IDxcBlobUtf16 *shader_name   = NULL;
		compiler_result->GetOutput(DXC_OUT_OBJECT, IID_IDxcBlob, (void **)&shader_buffer, &shader_name);
		if (shader_buffer == NULL) {
			return 1;
		}
		else {
			*outputlength = shader_buffer->GetBufferSize();
			*output       = (uint8_t *)malloc(*outputlength);
			memcpy(*output, shader_buffer->GetBufferPointer(), shader_buffer->GetBufferSize());
		}

		/*
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
		*/

		return 0;
	}
	else {
		return 1;
	}
#else
	return 1;
#endif
}
