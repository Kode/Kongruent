#include "../shader_stage.h"

#ifdef _WIN32

#include "../log.h"

#include <Windows.h>
#include <d3d9.h>

#include "d3dx9_mini.h"

typedef HRESULT(WINAPI *D3DXCompileShaderFromFileAType)(LPCSTR pSrcFile, CONST D3DXMACRO *pDefines, LPD3DXINCLUDE pInclude, LPCSTR pFunctionName,
                                                        LPCSTR pProfile, DWORD Flags, LPD3DXBUFFER *ppShader, LPD3DXBUFFER *ppErrorMsgs,
                                                        LPD3DXCONSTANTTABLE *ppConstantTable);

static D3DXCompileShaderFromFileAType CompileShaderFromFileA = NULL;

#endif

int compileHLSLToD3D9(const char *from, const char *to, const char *source, char *output, int *outputlength, void *attributes, shader_stage stage) {
#ifdef _WIN32
	HMODULE lib = LoadLibraryA("d3dx9_43.dll");
	if (lib != NULL) CompileShaderFromFileA = (D3DXCompileShaderFromFileAType)GetProcAddress(lib, "D3DXCompileShaderFromFileA");

	if (CompileShaderFromFileA == NULL) {
		kong_log(LOG_LEVEL_ERROR, "d3dx9_43.dll could not be loaded, please install dxwebsetup.");
		return 1;
	}

	LPD3DXBUFFER errors;
	LPD3DXBUFFER shader;
	LPD3DXCONSTANTTABLE table;
	HRESULT hr = CompileShaderFromFileA(from, NULL, NULL, "main", stage == SHADER_STAGE_VERTEX ? "vs_2_0" : "ps_2_0", 0, &shader, &errors, &table);
	if (FAILED(hr)) hr = CompileShaderFromFileA(from, NULL, NULL, "main", stage == SHADER_STAGE_VERTEX ? "vs_3_0" : "ps_3_0", 0, &shader, &errors, &table);
	if (errors != NULL) {
		kong_log(LOG_LEVEL_ERROR, "%s", (char *)errors->lpVtbl->GetBufferPointer((IDirect3DCryptoSession9 *)errors));
	}
	if (!FAILED(hr)) {
		/*std::ofstream file(to, std::ios_base::binary);

		file.put((char)attributes.size());
		for (std::map<std::string, int>::const_iterator attribute = attributes.begin(); attribute != attributes.end(); ++attribute) {
		    file << attribute->first.c_str();
		    file.put(0);
		    file.put(attribute->second);
		}

		D3DXCONSTANTTABLE_DESC desc;
		table->GetDesc(&desc);
		file.put(desc.Constants);
		for (UINT i = 0; i < desc.Constants; ++i) {
		    D3DXHANDLE handle = table->GetConstant(NULL, i);
		    D3DXCONSTANT_DESC descriptions[10];
		    UINT count = 10;
		    table->GetConstantDesc(handle, descriptions, &count);
		    if (count > 1) {
		        log(LOG_LEVEL_ERROR, "Error: Number of descriptors for one constant is greater than one.");
		    }
		    for (UINT i2 = 0; i2 < count; ++i2) {
		        char regtype;
		        switch (descriptions[i2].RegisterSet) {
		        case D3DXRS_BOOL:
		            regtype = 'b';
		            break;
		        case D3DXRS_INT4:
		            regtype = 'i';
		            break;
		        case D3DXRS_FLOAT4:
		            regtype = 'f';
		            break;
		        case D3DXRS_SAMPLER:
		            regtype = 's';
		            break;
		        }
		        // std::cout << descriptions[i2].Name << " " << regtype << descriptions[i2].RegisterIndex << " " << descriptions[i2].RegisterCount << std::endl;
		        file << descriptions[i2].Name;
		        file.put(0);
		        file.put(regtype);
		        file.put(descriptions[i2].RegisterIndex);
		        file.put(descriptions[i2].RegisterCount);
		    }
		}
		DWORD *data = (DWORD *)shader->GetBufferPointer();
		for (unsigned i = 0; i < shader->GetBufferSize() / 4; ++i) {
		    if ((data[i] & 0xffff) == 0xfffe) { // comment token
		        unsigned size = (data[i] >> 16) & 0xffff;
		        i += size;
		    }
		    else
		        file.write((char *)&data[i], 4);
		}*/
		// file.write((char*)shader->GetBufferPointer(), shader->GetBufferSize());
		return 0;
	}
	else {
		kong_log(LOG_LEVEL_ERROR, "%s", (char *)errors->lpVtbl->GetBufferPointer((IDirect3DCryptoSession9 *)errors));
		return 1;
	}
#else
	return 1;
#endif
}
