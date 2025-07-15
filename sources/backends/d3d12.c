#include "d3d12.h"

#include "../errors.h"

#ifdef _WIN32

#include "../log.h"

#include "dxcapi-c.h"

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

	unsigned __int64 length  = errors->lpVtbl->GetStringLength(errors);
	char            *pointer = errors->lpVtbl->GetStringPointer(errors);

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
