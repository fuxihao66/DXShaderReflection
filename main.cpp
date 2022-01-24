#include <DirectXMath.h> // For XMVector, XMFLOAT3, XMFLOAT4
#include <comdef.h>      // for _com_error
#include <d3d12.h>       // for D3D12 interface
#include <dxgi1_6.h>     // for DXGI
#include <wrl.h>         // for Microsoft::WRL::ComPtr
#include <vector>
#include <map>
#include <memory>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <atlbase.h>        // Common COM helpers.
#include <iostream>
#include <d3d11shader.h>
#include "dxcapi.h"  
#include "d3dx12.h"

enum ShaderModel {
    SHADER_MODEL_1,
    SHADER_MODEL_2,
    SHADER_MODEL_3,
    SHADER_MODEL_4,
    SHADER_MODEL_5,
    SHADER_MODEL_5_1,
    SHADER_MODEL_6_0,
    SHADER_MODEL_6_1,
    SHADER_MODEL_6_2,
    SHADER_MODEL_6_3,
    SHADER_MODEL_6_4,
    SHADER_MODEL_6_5
};
inline std::string WStr2Str(LPCWSTR wstr) {
    int nLen = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);

    if (nLen <= 0) return std::string("");

    char* pszDst = new char[nLen];
    if (NULL == pszDst) return std::string("");

    WideCharToMultiByte(CP_ACP, 0, wstr, -1, pszDst, nLen, NULL, NULL);
    pszDst[nLen - 1] = 0;

    std::string strTemp(pszDst);
    delete[] pszDst;

    return strTemp;
}



//shaderType cs ps lib vs gs
inline std::wstring GetPTarget(const std::wstring& shaderType, ShaderModel sm) {
    auto pTarget = shaderType;

    switch (sm) {
    case SHADER_MODEL_1:
        pTarget += L"_1_1";
        break;
    case SHADER_MODEL_2:
        pTarget += L"_2_0";
        break;
    case SHADER_MODEL_3:
        pTarget += L"_3_0";
        break;
    case SHADER_MODEL_4:
        pTarget += L"_4_1";
        break;
    case SHADER_MODEL_5:
        pTarget += L"_5_0";
        break;
    case SHADER_MODEL_5_1:
        pTarget += L"_5_0";
        break;
    default:
        pTarget += L"_6_0";
        break;
    }

    return pTarget;
}

void CreateFromFileFXC(const std::wstring& filename, const std::wstring& entryPoint, const std::wstring& pTarget) {
    ID3DBlob* pPSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    auto strPTarget = WStr2Str(pTarget.c_str());
    auto strEntryPoint = WStr2Str(entryPoint.c_str());




    D3DCompileFromFile(filename.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, strEntryPoint.c_str(), strPTarget.c_str(), 0, 0, &pPSBlob, &pErrorBlob);




    if (pErrorBlob)
    {
        OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
        pErrorBlob->Release();
    }

    ID3D12ShaderReflection* pReflection = NULL;
    D3DReflect(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), IID_ID3D12ShaderReflection, (void**)&pReflection);


    std::cout << WStr2Str(filename.c_str()) << std::endl;
    std::cout << WStr2Str(entryPoint.c_str()) << std::endl;
    std::cout << WStr2Str(pTarget.c_str()) << std::endl;
    //
    //RenderEngineD3D12Impl::Instance()->CreatePipeline(pPSBlob->GetBufferSize(), pPSBlob->GetBufferPointer(), _shaderName);
    //



    D3D12_SHADER_DESC shader_desc;
    pReflection->GetDesc(&shader_desc);

    for (int i = 0; i < shader_desc.BoundResources; i++)
    {
        D3D12_SHADER_INPUT_BIND_DESC  resource_desc;
        pReflection->GetResourceBindingDesc(i, &resource_desc);


        auto shaderVarName = resource_desc.Name;
        auto registerSpace = resource_desc.Space;
        auto resourceType = resource_desc.Type;
        auto registerIndex = resource_desc.BindPoint;
        //auto registerIndex = resource_desc.uID;

        std::cout << "var name is " << shaderVarName << std::endl;
        std::cout << "type name is " << resourceType << std::endl;
        std::cout << "register index is " << registerIndex << std::endl;
        std::cout << "register space is " << registerSpace << std::endl;

        if (resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_TBUFFER ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_STRUCTURED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_BYTEADDRESS ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_RTACCELERATIONSTRUCTURE) {
            _srvResourceBinding.push_back(BindingDesc{ shaderVarName , registerIndex, registerSpace });

        }
        else if (resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWTYPED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWBYTEADDRESS ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_APPEND_STRUCTURED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_CONSUME_STRUCTURED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_FEEDBACKTEXTURE) {
            _uavResourceBinding.push_back(BindingDesc{ shaderVarName , registerIndex, registerSpace });
        }
        else if (resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER) {
            _cbvResourceBinding.push_back(BindingDesc{ shaderVarName , registerIndex, registerSpace });

        }
        _space = registerSpace;

    }



}


// TODO: 反射不同
bool CreateFromFileDXR(const std::wstring& filename, const std::wstring& pTarget) {
    CComPtr<IDxcUtils> pUtils;
    CComPtr<IDxcCompiler3> pCompiler;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

    //
    // Create default include handler. (You can create your own...)
    //
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);


    //
    // COMMAND LINE: dxc myshader.hlsl -E main -T ps_6_0 -Zi -D MYDEFINE=1 -Fo myshader.bin -Fd myshader.pdb -Qstrip_reflect
    //
    LPCWSTR pszArgs[] =
    {
        filename.c_str(),            // Optional shader source file name for error reporting and for PIX shader source view.  
        //L"-E", L"main",              // Entry point.
        //L"-T", L"ps_6_0",            // Target.
        //L"-E", entryPoint.c_str(),              // Entry point.
        L"-T", pTarget.c_str(),            // Target.
        L"-Zi",                      // Enable debug information.
        L"-D", L"MYDEFINE=1",        // A single define.
        L"-Fo", L"myshader.bin",     // Optional. Stored in the pdb. 
        L"-Fd", L"myshader.pdb",     // The file name of the pdb. This must either be supplied or the autogenerated file name must be used.
        L"-Qstrip_reflect",          // Strip reflection into a separate blob. 
    };


    //
    // Open source file.  
    //
    CComPtr<IDxcBlobEncoding> pSource = nullptr;
    pUtils->LoadFile(filename.c_str(), nullptr, &pSource);
    DxcBuffer Source;
    Source.Ptr = pSource->GetBufferPointer();
    Source.Size = pSource->GetBufferSize();
    Source.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.


    //
    // Compile it with specified arguments.
    //
    CComPtr<IDxcResult> pResults;
    pCompiler->Compile(
        &Source,                // Source buffer.
        pszArgs,                // Array of pointers to arguments.
        _countof(pszArgs),      // Number of arguments.
        pIncludeHandler,        // User-provided interface to handle #include directives (optional).
        IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
    );

    //
    // Print errors if present.
    //
    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    // Note that d3dcompiler would return null if no errors or warnings are present.  
    // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
    if (pErrors != nullptr && pErrors->GetStringLength() != 0)
        wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());

    //
    // Quit if the compilation failed.
    //
    HRESULT hrStatus;
    pResults->GetStatus(&hrStatus);
    if (FAILED(hrStatus))
    {
        wprintf(L"Compilation Failed\n");
        return false;
    }

    //
    // Save shader binary.
    //
    CComPtr<IDxcBlob> pShader = nullptr;
    CComPtr<IDxcBlobUtf16> pShaderName = nullptr;
    pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), &pShaderName);
    if (pShader != nullptr)
    {
        //RenderEngineD3D12Impl::Instance()->CreateRTPipeline(pShader->GetBufferSize(), pShader->GetBufferPointer(), _shaderName);

    }

    // reflection
    CComPtr< ID3D12LibraryReflection > pReflection;  // 需要用不同的reflection


    CComPtr<IDxcBlob> pReflectionData;
    pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);
    if (pReflectionData != nullptr)
    {
        // Optionally, save reflection blob for later here.

        // Create reflection interface.
        DxcBuffer ReflectionData;
        ReflectionData.Encoding = DXC_CP_ACP;
        ReflectionData.Ptr = pReflectionData->GetBufferPointer();
        ReflectionData.Size = pReflectionData->GetBufferSize();

        pUtils->CreateReflection(&ReflectionData, IID_PPV_ARGS(&pReflection));

        // Use reflection interface here.

    }


    D3D12_LIBRARY_DESC libdesc;

    pReflection->GetDesc(&libdesc);


    for (auto i = 0; i < libdesc.FunctionCount; i++) {
        // 先hit miss 最后 raygen
        auto functionReflect = pReflection->GetFunctionByIndex(i);

        D3D12_FUNCTION_DESC  shader_desc;
        functionReflect->GetDesc(&shader_desc);


        for (int i = 0; i < shader_desc.BoundResources; i++)
        {
            D3D12_SHADER_INPUT_BIND_DESC  resource_desc;
            functionReflect->GetResourceBindingDesc(i, &resource_desc);


            auto shaderVarName = resource_desc.Name;
            auto registerSpace = resource_desc.Space;
            auto resourceType = resource_desc.Type;
            //auto registerIndex = resource_desc.uID;
            auto registerIndex = resource_desc.BindPoint;

            std::cout << "var name is " << shaderVarName << std::endl;
            std::cout << "type name is " << resourceType << std::endl;
            std::cout << "register index is " << registerIndex << std::endl;
            std::cout << "register space is " << registerSpace << std::endl;

            if (resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_TBUFFER ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_STRUCTURED ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_BYTEADDRESS ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_RTACCELERATIONSTRUCTURE) {
                _srvResourceBinding.push_back(BindingDesc{ shaderVarName , registerIndex, registerSpace });

            }
            else if (resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWTYPED ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWBYTEADDRESS ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_APPEND_STRUCTURED ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_CONSUME_STRUCTURED ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER ||
                resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_FEEDBACKTEXTURE) {
                _uavResourceBinding.push_back(BindingDesc{ shaderVarName , registerIndex, registerSpace });
            }
            else if (resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER) {
                _cbvResourceBinding.push_back(BindingDesc{ shaderVarName , registerIndex, registerSpace });

            }
            _space = registerSpace;
        }
    }



    return true;

}
bool CreateFromFileDXC(const std::wstring& filename, const std::wstring& entryPoint, const std::wstring& pTarget) {

    CComPtr<IDxcUtils> pUtils;
    CComPtr<IDxcCompiler3> pCompiler;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

    //
    // Create default include handler. (You can create your own...)
    //
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

    std::cout << WStr2Str(filename.c_str()) << std::endl;
    std::cout << WStr2Str(entryPoint.c_str()) << std::endl;
    std::cout << WStr2Str(pTarget.c_str()) << std::endl;

    //
    // COMMAND LINE: dxc myshader.hlsl -E main -T ps_6_0 -Zi -D MYDEFINE=1 -Fo myshader.bin -Fd myshader.pdb -Qstrip_reflect
    //
    LPCWSTR pszArgs[] =
    {
        filename.c_str(),            // Optional shader source file name for error reporting and for PIX shader source view.  
        //L"-E", L"main",              // Entry point.
        //L"-T", L"ps_6_0",            // Target.
        L"-E", entryPoint.c_str(),              // Entry point.
        L"-T", pTarget.c_str(),            // Target.
        L"-Zi",                      // Enable debug information.
        L"-D", L"MYDEFINE=1",        // A single define.
        L"-Fo", L"myshader.bin",     // Optional. Stored in the pdb. 
        L"-Fd", L"myshader.pdb",     // The file name of the pdb. This must either be supplied or the autogenerated file name must be used.
        L"-Qstrip_reflect",          // Strip reflection into a separate blob. 
    };


    //
    // Open source file.  
    //
    CComPtr<IDxcBlobEncoding> pSource = nullptr;
    pUtils->LoadFile(filename.c_str(), nullptr, &pSource);
    DxcBuffer Source;
    Source.Ptr = pSource->GetBufferPointer();
    Source.Size = pSource->GetBufferSize();
    Source.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.


    //
    // Compile it with specified arguments.
    //
    CComPtr<IDxcResult> pResults;
    pCompiler->Compile(
        &Source,                // Source buffer.
        pszArgs,                // Array of pointers to arguments.
        _countof(pszArgs),      // Number of arguments.
        pIncludeHandler,        // User-provided interface to handle #include directives (optional).
        IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
    );

    //
    // Print errors if present.
    //
    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    // Note that d3dcompiler would return null if no errors or warnings are present.  
    // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
    if (pErrors != nullptr && pErrors->GetStringLength() != 0)
        wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());

    //
    // Quit if the compilation failed.
    //
    HRESULT hrStatus;
    pResults->GetStatus(&hrStatus);
    if (FAILED(hrStatus))
    {
        wprintf(L"Compilation Failed\n");
        return false;
    }

    //
    // Save shader binary.
    //
    CComPtr<IDxcBlob> pShader = nullptr;
    CComPtr<IDxcBlobUtf16> pShaderName = nullptr;
    pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), &pShaderName);
    if (pShader != nullptr)
    {
        //RenderEngineD3D12Impl::Instance()->CreatePipeline(pShader->GetBufferSize(), pShader->GetBufferPointer(), _shaderName);

    }

    CComPtr< ID3D12ShaderReflection > pReflection;

    CComPtr<IDxcBlob> pReflectionData;
    pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);
    if (pReflectionData != nullptr)
    {
        // Optionally, save reflection blob for later here.

        // Create reflection interface.
        DxcBuffer ReflectionData;
        ReflectionData.Encoding = DXC_CP_ACP;
        ReflectionData.Ptr = pReflectionData->GetBufferPointer();
        ReflectionData.Size = pReflectionData->GetBufferSize();

        pUtils->CreateReflection(&ReflectionData, IID_PPV_ARGS(&pReflection));

        // Use reflection interface here.

    }


    D3D12_SHADER_DESC shader_desc;
    pReflection->GetDesc(&shader_desc);

    for (int i = 0; i < shader_desc.BoundResources; i++)
    {
        D3D12_SHADER_INPUT_BIND_DESC  resource_desc;
        pReflection->GetResourceBindingDesc(i, &resource_desc);


        auto shaderVarName = resource_desc.Name;
        auto registerSpace = resource_desc.Space;
        auto resourceType = resource_desc.Type;
        auto registerIndex = resource_desc.BindPoint;

        std::cout << "var name is " << shaderVarName << std::endl;
        std::cout << "type name is " << resourceType << std::endl;
        //std::cout << "register index is " << registerIndex << std::endl;
        std::cout << "register space is " << registerSpace << std::endl;

        std::cout << "bind point is " << resource_desc.BindPoint << std::endl;
        if (resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_TBUFFER ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_STRUCTURED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_BYTEADDRESS ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_RTACCELERATIONSTRUCTURE) {
            _srvResourceBinding.push_back(BindingDesc{ shaderVarName , registerIndex, registerSpace });

        }
        else if (resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWTYPED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWBYTEADDRESS ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_APPEND_STRUCTURED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_CONSUME_STRUCTURED ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER ||
            resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_FEEDBACKTEXTURE) {
            _uavResourceBinding.push_back(BindingDesc{ shaderVarName , registerIndex, registerSpace });
        }
        else if (resourceType == D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER) {
            _cbvResourceBinding.push_back(BindingDesc{ shaderVarName , registerIndex, registerSpace });


        }
    }

    return true;

}


int main() {
    //CreateFromFileDXC(L"C:/Users/admin/Desktop/shaderCompile/Comparison/DirectX-Graphics-Samples/Samples/Desktop/D3D12HDR/src/gradientPS.hlsl", L"PSMain", L"ps_6_0");
    //CreateFromFileDXR(L"C:/Users/admin/Desktop/shaderCompile/Comparison/DirectX-Graphics-Samples/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingSimpleLighting/Raytracing.hlsl", L"PSMain", L"ps_6_3");
    CreateFromFileFXC(L"C:/Users/admin/Desktop/shaderCompile/Comparison/DirectX-Graphics-Samples/Samples/Desktop/D3D12HDR/src/gradientPS.hlsl", L"PSMain", L"ps_5_1");

    return 0;
}