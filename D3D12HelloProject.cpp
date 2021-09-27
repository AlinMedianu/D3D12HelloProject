//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "D3D12HelloProject.h"

D3D12HelloProject::D3D12HelloProject(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    frameIndex(0),
    viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    renderTargetViewDescriptorSize{}, depthStencilViewDescriptorSize{}, shaderBufferResourceViewsDescriptorSize{},
    meshes{}, models{}, perSceneBuffer{},
    channelStencilTexture{},
    shaderResourceViewDefaultBuffers{}, shaderResourceViewUploadBuffers{}
{
}

void D3D12HelloProject::OnInit()
{
    LoadPipeline();
    LoadAssets();
    ThrowIfFailed(commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForPreviousFrame();
}

// Load the rendering pipeline dependencies.
void D3D12HelloProject::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            ComPtr<IDXGIInfoQueue> debugQueue;
            ThrowIfFailed(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugQueue)));
            debugQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            debugQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = frameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> baseSwapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &baseSwapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(baseSwapChain.As(&swapChain));
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        renderTargetViewDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        depthStencilViewDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        shaderBufferResourceViewsDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

        heapDesc.NumDescriptors = frameCount;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&renderTargetViewHeap)));

        heapDesc.NumDescriptors = 2;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&depthStencilViewHeap)));

        heapDesc.NumDescriptors = modelCount + 1;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&constantBufferViewHeap)));

        heapDesc.NumDescriptors = textureCount + 2;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&shaderResourceViewHeap)));
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < frameCount; n++)
        {
            ThrowIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
            device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, renderTargetViewDescriptorSize);
        }
    }

    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
}

// Load the sample assets.
void D3D12HelloProject::LoadAssets()
{
    // Create a root signature consisting of a descriptor table with a single CBV.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }
        CD3DX12_DESCRIPTOR_RANGE1 perSceneConstantBufferTable;
        CD3DX12_DESCRIPTOR_RANGE1 channelStencilShaderResourceTable;
        CD3DX12_DESCRIPTOR_RANGE1 textureTable;
        std::array<CD3DX12_ROOT_PARAMETER1, 4> rootParameters;
        perSceneConstantBufferTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        rootParameters[0].InitAsDescriptorTable(1, &perSceneConstantBufferTable);
        rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
        channelStencilShaderResourceTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
        rootParameters[2].InitAsDescriptorTable(1, &channelStencilShaderResourceTable, D3D12_SHADER_VISIBILITY_PIXEL);
        textureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        rootParameters[3].InitAsDescriptorTable(1, &textureTable, D3D12_SHADER_VISIBILITY_PIXEL);

        const CD3DX12_STATIC_SAMPLER_DESC pointWrap
        (
            0,
            D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP
        );
        const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap
        (
            1, 
            D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  
            0.0f,                             
            8
        );
        std::array<CD3DX12_STATIC_SAMPLER_DESC, 2> staticSamplers{ pointWrap, anisotropicWrap };

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(rootParameters.size(), rootParameters.data(), staticSamplers.size(), staticSamplers.data(), rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> litPixelShader;
        ComPtr<ID3DBlob> channelStencilReaderPixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ComPtr<ID3DBlob> errorBlob = nullptr;
        D3DCompileFromFile(GetAssetFullPath(L"Lit.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "Vertex", "vs_5_1", compileFlags, 0, &vertexShader, &errorBlob);
        if (errorBlob != nullptr)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob = nullptr;
        }
        const D3D_SHADER_MACRO maxNumberOfLightsDefines[] =
        {
            //"MAX_NUMBER_DIRECTIONAL_LIGHTS", "1",
            //"MAX_NUMBER_DIRECTIONAL_LIGHTS", "1",
            //"MAX_NUMBER_POINT_LIGHTS", "1",
            "MAX_NUMBER_SPOT_LIGHTS", "1",
            //"MAX_NUMBER_CAPSULE_LIGHTS", "1",
            NULL, NULL
        };
        D3DCompileFromFile(GetAssetFullPath(L"Lit.hlsl").c_str(), maxNumberOfLightsDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, "LitPixel", "ps_5_1", compileFlags, 0, &litPixelShader, &errorBlob);
        if (errorBlob != nullptr)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob = nullptr;
        }
        D3DCompileFromFile(GetAssetFullPath(L"Lit.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, 
            "ChannelStencilPixel", "ps_5_1", compileFlags, 0, &channelStencilReaderPixelShader, &errorBlob);
        if (errorBlob != nullptr)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob = nullptr;
        }

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueStateDescription = {};
        opaqueStateDescription.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        opaqueStateDescription.pRootSignature = rootSignature.Get();
        opaqueStateDescription.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        opaqueStateDescription.PS = CD3DX12_SHADER_BYTECODE(litPixelShader.Get());
        opaqueStateDescription.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        opaqueStateDescription.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);        
        opaqueStateDescription.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        opaqueStateDescription.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        opaqueStateDescription.SampleMask = UINT_MAX;
        opaqueStateDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        opaqueStateDescription.NumRenderTargets = 1;
        opaqueStateDescription.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        opaqueStateDescription.SampleDesc.Count = 1;
        ThrowIfFailed(device->CreateGraphicsPipelineState(&opaqueStateDescription,
            IID_PPV_ARGS(&pipelineStates[static_cast<size_t>(RenderLayer::Opaque)])));  
        D3D12_GRAPHICS_PIPELINE_STATE_DESC channelStencilWritterStateDescription = opaqueStateDescription;
        channelStencilWritterStateDescription.NumRenderTargets = 0;
        channelStencilWritterStateDescription.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
        CD3DX12_DEPTH_STENCIL_DESC channelStencilWritterDepthStencilDescription(D3D12_DEFAULT);
        channelStencilWritterDepthStencilDescription.StencilEnable = true;
        channelStencilWritterDepthStencilDescription.FrontFace.StencilFailOp = D3D12_STENCIL_OP_ZERO;
        channelStencilWritterDepthStencilDescription.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
        channelStencilWritterDepthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
        channelStencilWritterDepthStencilDescription.BackFace.StencilFailOp = D3D12_STENCIL_OP_ZERO;
        channelStencilWritterDepthStencilDescription.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
        channelStencilWritterDepthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
        channelStencilWritterStateDescription.DepthStencilState = channelStencilWritterDepthStencilDescription;
        ThrowIfFailed(device->CreateGraphicsPipelineState(&channelStencilWritterStateDescription,
            IID_PPV_ARGS(&pipelineStates[static_cast<size_t>(RenderLayer::ChannelStencilWritter)])));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC channelStencilReaderStateDescription = opaqueStateDescription;
        channelStencilReaderStateDescription.PS = CD3DX12_SHADER_BYTECODE(channelStencilReaderPixelShader.Get());
        ThrowIfFailed(device->CreateGraphicsPipelineState(&channelStencilReaderStateDescription,
            IID_PPV_ARGS(&pipelineStates[static_cast<size_t>(RenderLayer::ChannelStencilReader)])));
        D3D12_RENDER_TARGET_BLEND_DESC  transparencyBlend{};
        transparencyBlend.BlendEnable = true;
        transparencyBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        transparencyBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        transparencyBlend.BlendOp = D3D12_BLEND_OP_ADD;
        transparencyBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        transparencyBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        transparencyBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        transparencyBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC transparencyStateDescription = opaqueStateDescription;
        transparencyStateDescription.BlendState.RenderTarget[0] = transparencyBlend;
        ThrowIfFailed(device->CreateGraphicsPipelineState(&transparencyStateDescription,
            IID_PPV_ARGS(&pipelineStates[static_cast<size_t>(RenderLayer::Transparent)])));
    }

    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
    ThrowIfFailed(CreateDDSTextureFromFile12(device.Get(), commandList.Get(), L"Textures/tile.dds",
        shaderResourceViewDefaultBuffers[0], shaderResourceViewUploadBuffers[0]));
    ThrowIfFailed(CreateDDSTextureFromFile12(device.Get(), commandList.Get(), L"Textures/bricks2.dds",
        shaderResourceViewDefaultBuffers[1], shaderResourceViewUploadBuffers[1]));
    ThrowIfFailed(CreateDDSTextureFromFile12(device.Get(), commandList.Get(), L"Textures/checkboard.dds",
        shaderResourceViewDefaultBuffers[2], shaderResourceViewUploadBuffers[2]));


    std::ifstream meshLoader("Models/skull.txt");
    std::string ignore;
    UINT triangleCount;
    UINT vertexCount;
    meshLoader >> ignore >> vertexCount;
    meshLoader >> ignore >> triangleCount;
    meshLoader >> ignore >> ignore >> ignore >> ignore;
    std::vector<PositionNormalUV> vertices(vertexCount);
    for (UINT i = 0; i < vertexCount; ++i)
    {
        meshLoader >> vertices[i].position.x >> vertices[i].position.y >> vertices[i].position.z;
        meshLoader >> vertices[i].normal.x >> vertices[i].normal.y >> vertices[i].normal.z;
        vertices[i].uv = { 0.0f, 0.0f };
    }
    meshLoader >> ignore >> ignore >> ignore;
    std::vector<UINT> indices(3 * triangleCount);
    for (UINT i = 0; i < triangleCount; ++i)
    {
        meshLoader >> indices[i * 3U + 0] >> indices[i * 3U + 1] >> indices[i * 3U + 2];
    }
    meshLoader.close(); 
    //MeshData skull{ vertices , indices };
    MeshData grid{}, cube{};
    CreateGrid(3, 3, 2, 2, grid);
    CreateParallelepiped(1, 1, 1, cube);
    CreateMesh(grid, meshes[static_cast<size_t>(MeshType::Grid)]);
    CreateMesh(cube, meshes[static_cast<size_t>(MeshType::Parallelepiped)]);
    //CreateMesh(skull, meshes[static_cast<size_t>(MeshType::Skull)]);
    CD3DX12_CPU_DESCRIPTOR_HANDLE constantBufferDescriptorHandle(constantBufferViewHeap->GetCPUDescriptorHandleForHeapStart());
    CreateConstantBuffer(constantBufferDescriptorHandle, perSceneBuffer);
    for (size_t meshIndex = 0, firstModelPerMeshIndex = 0; meshIndex < meshCount; firstModelPerMeshIndex += modelsPerMesh[meshIndex++])
        for (size_t modelIndex = firstModelPerMeshIndex; modelIndex < modelsPerMesh[meshIndex] + firstModelPerMeshIndex; ++modelIndex)
        {
            models[modelIndex].mesh = &meshes[meshIndex];
            CreateConstantBuffer(constantBufferDescriptorHandle, models[modelIndex].buffer);     
            models[modelIndex].renderLayer = RenderLayer::Transparent;
        }
    auto& perSceneData = perSceneBuffer.data.data;
    perSceneData.cameraPosition = { 0, 1, -5 };
    cameraForward = { 0, 0, 1 };
    cameraRight = { 1, 0, 0 };
    cameraUp = { 0, 1, 0 };
    XMMATRIX cameraView = XMMatrixLookToLH(XMLoadFloat3(&perSceneBuffer.data.data.cameraPosition), XMLoadFloat3(&cameraForward), XMLoadFloat3(&cameraUp));
    XMMATRIX cameraProjection = XMMatrixPerspectiveFovLH(0.25f * std::numbers::pi_v<float>, m_aspectRatio, 1, 1000);
    perSceneBuffer.data.data.viewProjection = XMMatrixTranspose(cameraView * cameraProjection);
    //perSceneData.ambientalLight.downColour = { 1, 0, 0 };
    //perSceneData.ambientalLight.colourDifference = { -1, 1, 0 };
    //perSceneData.directionalLights[0].colour = { 0.5f, 0.5f, 0 };
    //perSceneData.directionalLights[0].normalizedInvertedDirection = { 0, 0, -1 };
    /*perSceneData.pointLights[0].colour = { 0.5f, 0.5f, 0 };
    perSceneData.pointLights[0].position = { 0, 0, 0 };
    perSceneData.pointLights[0].rangeReciprocal = 0;*/
    perSceneData.spotLights[0].colour = { 1, 1, 0 };
    perSceneData.spotLights[0].position = { 0, 1, -3 };
    perSceneData.spotLights[0].normalizedInvertedDirection = { 0, 0, -1 };
    perSceneData.spotLights[0].rangeReciprocal = 0.01f;
    perSceneData.spotLights[0].cosOuterCone = std::cosf(std::numbers::pi_v<float> / 2);
    perSceneData.spotLights[0].cosInnerConeReciprocal = 1 / std::cosf(std::numbers::pi_v<float> / 4);
    /*perSceneData.capsuleLights.colour = { 0.5f, 0.5f, 0 };
    perSceneData.capsuleLights.segmentStartPosition = { -1, 0, -1 };
    perSceneData.capsuleLights.normalizedSegmentStartToSegmentEnd = { 1, 0, 0 };
    perSceneData.capsuleLights.segmentLength = 2;
    perSceneData.capsuleLights.rangeReciprocal = 1;*/
    perSceneBuffer.Update();
    for (size_t modelIndex = 0; modelIndex < modelCount; ++modelIndex)
    {
        auto& perModelData = models[modelIndex].buffer.data.data;
        perModelData.textureTransform = XMMatrixTranspose(XMMatrixIdentity());
        XMStoreFloat4(&perModelData.diffuseColour, Colors::White);
        perModelData.diffuseColour.w = 0.5f;
        perModelData.specularExponent = 100;
        perModelData.specularIntensity = 10;
    }
    models[0].buffer.data.data.model = XMMatrixTranspose(XMMatrixTranslation(0, 1.5f, 0) * XMMatrixTranslation(0, 0, 10));
    models[1].buffer.data.data.model = XMMatrixTranspose(XMMatrixRotationZ(-std::numbers::pi_v<float> / 2) * XMMatrixTranslation(1.5f, 0, 0) * XMMatrixTranslation(0, 0, 10));
    models[2].buffer.data.data.model = XMMatrixTranspose(XMMatrixRotationX(std::numbers::pi_v<float> / 2) * XMMatrixTranslation(0, 0, 1.5f) * XMMatrixTranslation(0, 0, 10));
    models[3].buffer.data.data.model = XMMatrixTranspose(XMMatrixRotationZ(std::numbers::pi_v<float>) * XMMatrixTranslation(0, -1.5f, 0) * XMMatrixTranslation(0, 0, 10));
    models[4].buffer.data.data.model = XMMatrixTranspose(XMMatrixRotationZ(std::numbers::pi_v<float> / 2) * XMMatrixTranslation(-1.5f, 0, 0) * XMMatrixTranslation(0, 0, 10));
    models[5].buffer.data.data.model = XMMatrixTranspose(XMMatrixRotationX(-std::numbers::pi_v<float> / 2) * XMMatrixTranslation(0, 0, -1.5f) * XMMatrixTranslation(0, 0, 10));
    for (size_t modelIndex = 6; modelIndex < 9; ++modelIndex)
        models[modelIndex].renderLayer = RenderLayer::ChannelStencilReader;
    XMStoreFloat4(&models[6].buffer.data.data.diffuseColour, Colors::Red);
    models[6].buffer.data.data.model = XMMatrixTranspose(XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(-1, 0, 0) * XMMatrixTranslation(0, 0, 10));
    XMStoreFloat4(&models[7].buffer.data.data.diffuseColour, Colors::Green);
    models[7].buffer.data.data.model = XMMatrixTranspose(XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0, 0, 10));
    XMStoreFloat4(&models[8].buffer.data.data.diffuseColour, Colors::Blue);
    models[8].buffer.data.data.model = XMMatrixTranspose(XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(1, 0, 0) * XMMatrixTranslation(0, 0, 10));
    XMStoreFloat4(&models[9].buffer.data.data.diffuseColour, Colors::White);
    models[9].renderLayer = RenderLayer::Opaque;
    models[9].buffer.data.data.model = XMMatrixTranspose(XMMatrixScaling(0.3f, 0.3f, 0.3f));
    for (size_t modelIndex = 0; modelIndex < modelCount; ++modelIndex)
        models[modelIndex].buffer.Update();
    std::sort(models.begin(), models.end(), [](const Model& first, const Model& second)
    {
        return first.renderLayer < second.renderLayer;
    });

    D3D12_RESOURCE_DESC channelStencilTextureDescription;
    ZeroMemory(&channelStencilTextureDescription, sizeof(D3D12_RESOURCE_DESC));
    channelStencilTextureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    channelStencilTextureDescription.Alignment = 0;
    channelStencilTextureDescription.Width = m_width;
    channelStencilTextureDescription.Height = m_height;
    channelStencilTextureDescription.DepthOrArraySize = 1;
    channelStencilTextureDescription.MipLevels = 1;
    channelStencilTextureDescription.Format = DXGI_FORMAT_R24G8_TYPELESS;
    channelStencilTextureDescription.SampleDesc.Count = 1;
    channelStencilTextureDescription.SampleDesc.Quality = 0;
    channelStencilTextureDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    channelStencilTextureDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    CD3DX12_HEAP_PROPERTIES defaultProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultProperties,
        D3D12_HEAP_FLAG_NONE,
        &channelStencilTextureDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&channelStencilTexture)));


    CD3DX12_CPU_DESCRIPTOR_HANDLE shaderResourceViewDescriptorHandle(shaderResourceViewHeap->GetCPUDescriptorHandleForHeapStart());
    D3D12_SHADER_RESOURCE_VIEW_DESC channelStencilShaderResourceViewDescription = {};
    channelStencilShaderResourceViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    channelStencilShaderResourceViewDescription.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
    channelStencilShaderResourceViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    channelStencilShaderResourceViewDescription.Texture2D.MostDetailedMip = 0;
    channelStencilShaderResourceViewDescription.Texture2D.MipLevels = 1;
    channelStencilShaderResourceViewDescription.Texture2D.ResourceMinLODClamp = 0.0f;
    channelStencilShaderResourceViewDescription.Texture2D.PlaneSlice = 1;
    device->CreateShaderResourceView(channelStencilTexture.Get(), &channelStencilShaderResourceViewDescription, shaderResourceViewDescriptorHandle);
    shaderResourceViewDescriptorHandle.Offset(1, shaderBufferResourceViewsDescriptorSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC emptyShaderResourceViewDescription = {};
    emptyShaderResourceViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    emptyShaderResourceViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    emptyShaderResourceViewDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    emptyShaderResourceViewDescription.Texture2D.MostDetailedMip = 0;
    emptyShaderResourceViewDescription.Texture2D.MipLevels = 1;
    emptyShaderResourceViewDescription.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(nullptr, &emptyShaderResourceViewDescription, shaderResourceViewDescriptorHandle);
    shaderResourceViewDescriptorHandle.Offset(1, shaderBufferResourceViewsDescriptorSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription = {};
    shaderResourceViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    shaderResourceViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDescription.Texture2D.MostDetailedMip = 0;
    shaderResourceViewDescription.Texture2D.ResourceMinLODClamp = 0.0f;
    for (size_t textureIndex = 0; textureIndex < textureCount; ++textureIndex)
    {
        shaderResourceViewDescription.Format = shaderResourceViewDefaultBuffers[textureIndex]->GetDesc().Format;
        shaderResourceViewDescription.Texture2D.MipLevels = shaderResourceViewDefaultBuffers[textureIndex]->GetDesc().MipLevels;
        device->CreateShaderResourceView(shaderResourceViewDefaultBuffers[textureIndex].Get(), &shaderResourceViewDescription, shaderResourceViewDescriptorHandle);
        shaderResourceViewDescriptorHandle.Offset(1, shaderBufferResourceViewsDescriptorSize);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilViewDescriptorHandle(depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart());
    D3D12_RESOURCE_DESC depthStencilDescription = {};
    depthStencilDescription.MipLevels = 1;
    depthStencilDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDescription.Width = m_width;
    depthStencilDescription.Height = m_height;
    depthStencilDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    depthStencilDescription.DepthOrArraySize = 1;
    depthStencilDescription.SampleDesc.Count = 1;
    depthStencilDescription.SampleDesc.Quality = 0;
    depthStencilDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    D3D12_CLEAR_VALUE clearValue;
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDescription,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthStencilBuffer)));
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDescription;
    depthStencilViewDescription.Flags = D3D12_DSV_FLAG_NONE;
    depthStencilViewDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilViewDescription.Texture2D.MipSlice = 0;
    device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilViewDescription, depthStencilViewDescriptorHandle);
    depthStencilViewDescriptorHandle.Offset(1, depthStencilViewDescriptorSize);
    D3D12_DEPTH_STENCIL_VIEW_DESC channelStencilDepthStencilViewDescription;
    channelStencilDepthStencilViewDescription.Flags = D3D12_DSV_FLAG_NONE;
    channelStencilDepthStencilViewDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    channelStencilDepthStencilViewDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    channelStencilDepthStencilViewDescription.Texture2D.MipSlice = 0;
    device->CreateDepthStencilView(channelStencilTexture.Get(), &channelStencilDepthStencilViewDescription, depthStencilViewDescriptorHandle);

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    fenceValue = 1;
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
    WaitForPreviousFrame();
}

void D3D12HelloProject::CreateParallelepiped(float width, float height, float depth, MeshData& parallelepiped)
{
    std::vector<PositionNormalUV>& vertices = parallelepiped.vertices;
    vertices.resize(24);
    std::vector<UINT>& indices = parallelepiped.indices;
    indices.resize(36);
    float halfWidth = 0.5f * width;
    float halfHeight = 0.5f * height;
    float halfDepth = 0.5f * depth;
    XMFLOAT3 left{ -1, 0, 0 }, right{ 1, 0, 0 }, up{ 0, 1, 0 }, 
        down{ 0, -1, 0 }, forward{ 0, 0, 1 }, back{ 0, 0, -1 };
    XMFLOAT2 topLeft{ 0, 0 }, topRight{ 0, 1 }, bottomLeft{ 1, 0 }, bottomRight{ 1, 1 };
    vertices[0] = { { -halfWidth, -halfHeight, -halfDepth }, back, topRight };
    vertices[1] = { { -halfWidth,  halfHeight, -halfDepth }, back, topLeft };
    vertices[2] = { {  halfWidth,  halfHeight, -halfDepth }, back, bottomLeft };
    vertices[3] = { {  halfWidth, -halfHeight, -halfDepth }, back, bottomRight };
    indices[0] = 0; indices[1] = 1; indices[2] = 2;
    indices[3] = 0; indices[4] = 2; indices[5] = 3;
    vertices[4] = { { -halfWidth, -halfHeight,  halfDepth }, forward, bottomRight };
    vertices[5] = { {  halfWidth, -halfHeight,  halfDepth }, forward, topRight };
    vertices[6] = { {  halfWidth,  halfHeight,  halfDepth }, forward, topLeft };
    vertices[7] = { { -halfWidth,  halfHeight,  halfDepth }, forward, bottomLeft };
    indices[6] = 4; indices[7] = 5; indices[8] = 6;
    indices[9] = 4; indices[10] = 6; indices[11] = 7;
    vertices[8] = { { -halfWidth,  halfHeight, -halfDepth }, up, topRight };
    vertices[9] = { { -halfWidth,  halfHeight,  halfDepth }, up, topLeft };
    vertices[10] = { {  halfWidth,  halfHeight,  halfDepth }, up, bottomLeft };
    vertices[11] = { {  halfWidth,  halfHeight, -halfDepth }, up, bottomRight };
    indices[12] = 8; indices[13] = 9; indices[14] = 10;
    indices[15] = 8; indices[16] = 10; indices[17] = 11;
    vertices[12] = { { -halfWidth, -halfHeight, -halfDepth }, down, bottomRight };
    vertices[13] = { {  halfWidth, -halfHeight, -halfDepth }, down, topRight };
    vertices[14] = { {  halfWidth, -halfHeight,  halfDepth }, down, topLeft };
    vertices[15] = { { -halfWidth, -halfHeight,  halfDepth }, down, bottomLeft };
    indices[18] = 12; indices[19] = 13; indices[20] = 14;
    indices[21] = 12; indices[22] = 14; indices[23] = 15;
    vertices[16] = { { -halfWidth, -halfHeight,  halfDepth }, left, topRight };
    vertices[17] = { { -halfWidth,  halfHeight,  halfDepth }, left, topLeft };
    vertices[18] = { { -halfWidth,  halfHeight, -halfDepth }, left, bottomLeft };
    vertices[19] = { { -halfWidth, -halfHeight, -halfDepth }, left, bottomRight };
    indices[24] = 16; indices[25] = 17; indices[26] = 18;
    indices[27] = 16; indices[28] = 18; indices[29] = 19;
    vertices[20] = { {  halfWidth, -halfHeight, -halfDepth }, right, topRight };
    vertices[21] = { {  halfWidth,  halfHeight, -halfDepth }, right, topLeft };
    vertices[22] = { {  halfWidth,  halfHeight,  halfDepth }, right, bottomLeft };
    vertices[23] = { {  halfWidth, -halfHeight,  halfDepth }, right, bottomRight };
    indices[30] = 20; indices[31] = 21; indices[32] = 22;
    indices[33] = 20; indices[34] = 22; indices[35] = 23;
}

void D3D12HelloProject::CreateGrid(float width, float depth, UINT vertexColumnCount, UINT vertexRowsCount, MeshData& grid)
{
    float halfWidth = 0.5f * width;
    float halfDepth = 0.5f * depth;
    float cellWidth = width / (vertexColumnCount - 1);
    float cellDepth = depth / (vertexRowsCount - 1);
    float cellU = 1.0f / (vertexColumnCount - 1);
    float cellV = 1.0f / (vertexRowsCount - 1);
    std::vector<PositionNormalUV>& vertices = grid.vertices;
    vertices.resize(vertexColumnCount * vertexRowsCount);
    for (UINT vertexRow = 0; vertexRow < vertexRowsCount; ++vertexRow)
    {
        float z = halfDepth - vertexRow * cellDepth;
        for (UINT vertexColumn = 0; vertexColumn < vertexColumnCount; ++vertexColumn)
        {
            float x = -halfWidth + vertexColumn * cellWidth;
            vertices[vertexRow * vertexColumnCount + vertexColumn].position = { x, 0, z };
            vertices[vertexRow * vertexColumnCount + vertexColumn].normal = { 0, 1, 0 };
            vertices[vertexRow * vertexColumnCount + vertexColumn].uv = { vertexColumn * cellU, vertexRow * cellV };
        }
    }
    std::vector<UINT>& indices = grid.indices;
    const UINT verticesPerCell = 6;
    indices.resize((vertexColumnCount - 1) * (vertexRowsCount - 1) * verticesPerCell);
    for (UINT vertexRowsInBetween = 0, cell = 0; vertexRowsInBetween < vertexRowsCount - 1; ++vertexRowsInBetween)
    {
        for (UINT vertexColumnInBetween = 0; vertexColumnInBetween < vertexColumnCount - 1; ++vertexColumnInBetween, cell += verticesPerCell)
        {
            indices[cell] = vertexRowsInBetween * vertexColumnCount + vertexColumnInBetween;
            indices[cell + 1] = vertexRowsInBetween * vertexColumnCount + vertexColumnInBetween + 1;
            indices[cell + 2] = (vertexRowsInBetween + 1) * vertexColumnCount + vertexColumnInBetween;
            indices[cell + 3] = (vertexRowsInBetween + 1) * vertexColumnCount + vertexColumnInBetween;
            indices[cell + 4] = vertexRowsInBetween * vertexColumnCount + vertexColumnInBetween + 1;
            indices[cell + 5] = (vertexRowsInBetween + 1) * vertexColumnCount + vertexColumnInBetween + 1;
        }
    }
}

void D3D12HelloProject::CreateMesh(const MeshData& data, Mesh& mesh)
{
    assert(mesh.indexCount == 0);
    const UINT vertexBufferSize = sizeof(PositionNormalUV) * data.vertices.size();
    CD3DX12_HEAP_PROPERTIES uploadProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC vertexBufferDescription(CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize));
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadProperties,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mesh.vertexBuffer)));
    void* vertexDataBegin;
    const CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(mesh.vertexBuffer->Map(0, &readRange, &vertexDataBegin));
    memcpy(vertexDataBegin, &data.vertices[0], vertexBufferSize);
    mesh.vertexBuffer->Unmap(0, nullptr);
    mesh.vertexBufferView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
    mesh.vertexBufferView.StrideInBytes = sizeof(PositionNormalUV);
    mesh.vertexBufferView.SizeInBytes = vertexBufferSize;
    mesh.indexCount = data.indices.size();
    const UINT indexBufferSize = sizeof(UINT) * mesh.indexCount;
    CD3DX12_RESOURCE_DESC indexBufferDescription(CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize));
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadProperties,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mesh.indexBuffer)));
    void* indexDataBegin;
    ThrowIfFailed(mesh.indexBuffer->Map(0, &readRange, &indexDataBegin));
    memcpy(indexDataBegin, &data.indices[0], indexBufferSize);
    mesh.indexBuffer->Unmap(0, nullptr);
    mesh.indexBufferView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
    mesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    mesh.indexBufferView.SizeInBytes = indexBufferSize;
}

// Update frame-based values.
void D3D12HelloProject::OnUpdate()
{
    XMFLOAT3& cameraPosition = perSceneBuffer.data.data.cameraPosition;
    
    if (GetAsyncKeyState('W'))
    {
        XMVECTOR distance = XMVectorReplicate(0.1f);
        XMVECTOR forward = XMLoadFloat3(&cameraForward);
        XMVECTOR position = XMLoadFloat3(&cameraPosition);
        XMStoreFloat3(&cameraPosition, XMVectorMultiplyAdd(distance, forward, position));
    }
    if (GetAsyncKeyState('S'))
    {
        XMVECTOR distance = XMVectorReplicate(-0.1f);
        XMVECTOR forward = XMLoadFloat3(&cameraForward);
        XMVECTOR position = XMLoadFloat3(&cameraPosition);
        XMStoreFloat3(&cameraPosition, XMVectorMultiplyAdd(distance, forward, position));
    }
    if (GetAsyncKeyState('A'))
    {
        XMVECTOR distance = XMVectorReplicate(-0.1f);
        XMVECTOR right = XMLoadFloat3(&cameraRight);
        XMVECTOR position = XMLoadFloat3(&cameraPosition);
        XMStoreFloat3(&cameraPosition, XMVectorMultiplyAdd(distance, right, position));
    }
    if (GetAsyncKeyState('D'))
    {
        XMVECTOR distance = XMVectorReplicate(0.1f);
        XMVECTOR right = XMLoadFloat3(&cameraRight);
        XMVECTOR position = XMLoadFloat3(&cameraPosition);
        XMStoreFloat3(&cameraPosition, XMVectorMultiplyAdd(distance, right, position));
    }
    if (GetAsyncKeyState(VK_SPACE))
        cameraPosition.y += 0.1f;
    if (GetAsyncKeyState(VK_LSHIFT))
        cameraPosition.y -= 0.1f;
    XMMATRIX cameraView = XMMatrixLookToLH(XMLoadFloat3(&cameraPosition), XMLoadFloat3(&cameraForward), XMLoadFloat3(&cameraUp));
    XMMATRIX cameraProjection = XMMatrixPerspectiveFovLH(0.25f * std::numbers::pi_v<float>, m_aspectRatio, 1, 1000);
    perSceneBuffer.data.data.viewProjection = XMMatrixTranspose(cameraView * cameraProjection);
    perSceneBuffer.Update();
}

void D3D12HelloProject::OnMouseDown(WPARAM btnState, int x, int y)
{
    lastMousePosition.x = x;
    lastMousePosition.y = y;

    SetCapture(Win32Application::GetHwnd());
}

void D3D12HelloProject::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void D3D12HelloProject::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float mouseXChange = XMConvertToRadians(0.25f * (x - lastMousePosition.x));
        float mouseYChange = XMConvertToRadians(0.25f * (y - lastMousePosition.y));
        XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&cameraRight), mouseYChange);
        XMStoreFloat3(&cameraUp, XMVector3TransformNormal(XMLoadFloat3(&cameraUp), rotation));
        XMStoreFloat3(&cameraForward, XMVector3TransformNormal(XMLoadFloat3(&cameraForward), rotation));
        rotation = XMMatrixRotationY(mouseXChange);
        XMStoreFloat3(&cameraRight, XMVector3TransformNormal(XMLoadFloat3(&cameraRight), rotation));
        XMStoreFloat3(&cameraUp, XMVector3TransformNormal(XMLoadFloat3(&cameraUp), rotation));
        XMStoreFloat3(&cameraForward, XMVector3TransformNormal(XMLoadFloat3(&cameraForward), rotation));
    }

    lastMousePosition.x = x;
    lastMousePosition.y = y;
}

// Render the scene.
void D3D12HelloProject::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void D3D12HelloProject::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(fenceEvent);
}

// Fill the command list with all the render commands and dependent state.
void D3D12HelloProject::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(commandList->Reset(commandAllocator.Get(), pipelineStates[static_cast<size_t>(RenderLayer::Opaque)].Get()));

    commandList->SetGraphicsRootSignature(rootSignature.Get());

    ID3D12DescriptorHeap* descriptorHeaps[] = { constantBufferViewHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    CD3DX12_RESOURCE_BARRIER backBufferPresentToRenderTarget(CD3DX12_RESOURCE_BARRIER::Transition
    (
        renderTargets[frameIndex].Get(), 
        D3D12_RESOURCE_STATE_PRESENT, 
        D3D12_RESOURCE_STATE_RENDER_TARGET
    ));
    commandList->ResourceBarrier(1, &backBufferPresentToRenderTarget);
    
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    CD3DX12_GPU_DESCRIPTOR_HANDLE descriptorHandle(constantBufferViewHeap->GetGPUDescriptorHandleForHeapStart());
    commandList->SetGraphicsRootDescriptorTable(0, descriptorHandle);
    descriptorHeaps[0] = shaderResourceViewHeap.Get();
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    CD3DX12_GPU_DESCRIPTOR_HANDLE shaderResourceViewHandle(shaderResourceViewHeap->GetGPUDescriptorHandleForHeapStart(), 2, shaderBufferResourceViewsDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(3, shaderResourceViewHandle);



    CD3DX12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, renderTargetViewDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle(depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart());
    commandList->OMSetRenderTargets(1, &renderTargetViewHandle, false, &depthStencilViewHandle);
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(renderTargetViewHandle, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    commandList->SetPipelineState(pipelineStates[static_cast<size_t>(RenderLayer::Opaque)].Get());
    shaderResourceViewHandle.Offset(-1, shaderBufferResourceViewsDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(2, shaderResourceViewHandle);
    for (size_t modelIndex = 0; modelIndex < modelCount; ++modelIndex)
        if (models[modelIndex].renderLayer == RenderLayer::Opaque)
        {
            commandList->SetGraphicsRootConstantBufferView(1, models[modelIndex].buffer.dataGPU->GetGPUVirtualAddress());
            commandList->IASetVertexBuffers(0, 1, &models[modelIndex].mesh->vertexBufferView);
            commandList->IASetIndexBuffer(&models[modelIndex].mesh->indexBufferView);
            commandList->DrawIndexedInstanced(models[modelIndex].mesh->indexCount, 1, 0, 0, 0);
        }


    /*CD3DX12_RESOURCE_BARRIER channelStencilReadToDepthWrite(CD3DX12_RESOURCE_BARRIER::Transition
    (
        channelStencilTexture.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_DEPTH_WRITE
    ));
    commandList->ResourceBarrier(1, &channelStencilReadToDepthWrite);
    depthStencilViewHandle.Offset(1, depthStencilViewDescriptorSize);
    commandList->OMSetRenderTargets(0, nullptr, false, &depthStencilViewHandle);
    commandList->ClearDepthStencilView(depthStencilViewHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    commandList->SetPipelineState(pipelineStates[static_cast<size_t>(RenderLayer::ChannelStencilWritter)].Get());
    for (size_t modelIndex = 0; modelIndex < modelCount; ++modelIndex)
        if (models[modelIndex].renderLayer == RenderLayer::Transparent || models[modelIndex].renderLayer == RenderLayer::ChannelStencilReader)
        {
            UINT ref = 1 << ((modelIndex % 3) * 2);
            commandList->OMSetStencilRef(ref);
            commandList->SetGraphicsRootConstantBufferView(1, models[modelIndex].buffer.dataGPU->GetGPUVirtualAddress());
            commandList->IASetVertexBuffers(0, 1, &models[modelIndex].mesh->vertexBufferView);
            commandList->IASetIndexBuffer(&models[modelIndex].mesh->indexBufferView);
            commandList->DrawIndexedInstanced(models[modelIndex].mesh->indexCount, 1, 0, 0, 0);
        }
    CD3DX12_RESOURCE_BARRIER channelStencilDepthWriteToRead(CD3DX12_RESOURCE_BARRIER::Transition
    (
        channelStencilTexture.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    ));
    commandList->ResourceBarrier(1, &channelStencilDepthWriteToRead);



    commandList->SetPipelineState(pipelineStates[static_cast<size_t>(RenderLayer::ChannelStencilReader)].Get());
    shaderResourceViewHandle.Offset(-1, shaderBufferResourceViewsDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(2, shaderResourceViewHandle);
    depthStencilViewHandle.Offset(-1, depthStencilViewDescriptorSize);
    commandList->OMSetRenderTargets(1, &renderTargetViewHandle, false, &depthStencilViewHandle);
    commandList->ClearRenderTargetView(renderTargetViewHandle, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(depthStencilViewHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    for (size_t modelIndex = 0; modelIndex < modelCount; ++modelIndex)
        if (models[modelIndex].renderLayer == RenderLayer::ChannelStencilReader)
        { 
            commandList->SetGraphicsRootConstantBufferView(1, models[modelIndex].buffer.dataGPU->GetGPUVirtualAddress());
            commandList->IASetVertexBuffers(0, 1, &models[modelIndex].mesh->vertexBufferView);
            commandList->IASetIndexBuffer(&models[modelIndex].mesh->indexBufferView);
            commandList->DrawIndexedInstanced(models[modelIndex].mesh->indexCount, 1, 0, 0, 0);
        }



    commandList->SetPipelineState(pipelineStates[static_cast<size_t>(RenderLayer::Transparent)].Get());
    shaderResourceViewHandle.Offset(1, shaderBufferResourceViewsDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(2, shaderResourceViewHandle);
    for (size_t modelIndex = 0; modelIndex < modelCount; ++modelIndex)
        if(models[modelIndex].renderLayer == RenderLayer::Transparent)
        {
            commandList->SetGraphicsRootConstantBufferView(1, models[modelIndex].buffer.dataGPU->GetGPUVirtualAddress());
            commandList->IASetVertexBuffers(0, 1, &models[modelIndex].mesh->vertexBufferView);
            commandList->IASetIndexBuffer(&models[modelIndex].mesh->indexBufferView);
            commandList->DrawIndexedInstanced(models[modelIndex].mesh->indexCount, 1, 0, 0, 0);
        }*/
    CD3DX12_RESOURCE_BARRIER backBufferRenderTargetToPresent(CD3DX12_RESOURCE_BARRIER::Transition
    (
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    ));
    commandList->ResourceBarrier(1, &backBufferRenderTargetToPresent);

    ThrowIfFailed(commandList->Close());
}

void D3D12HelloProject::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 previousFenceValue = fenceValue;
    ThrowIfFailed(commandQueue->Signal(fence.Get(), previousFenceValue));
    fenceValue++;

    // Wait until the previous frame is finished.
    if (fence->GetCompletedValue() < previousFenceValue)
    {
        ThrowIfFailed(fence->SetEventOnCompletion(previousFenceValue, fenceEvent));
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    frameIndex = swapChain->GetCurrentBackBufferIndex();
}
