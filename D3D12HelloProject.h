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

#pragma once

#include "DXSample.h"
#include <fstream>
#include <array>
#include <numbers>
#include <algorithm>
#include <numeric>
#include <DirectXColors.h>
#include "DDSTextureLoader.h"
#include <dxgidebug.h>

//REMOVE MACROS WHEN MSVC DECIDES TO SUPPORT THE no_unique_address ATTRIBUTE
#define USE_HEMISPHERIC_AMBIENTAL_LIGHTING false
#define MAX_NUMBER_DIRECTIONAL_LIGHTS 0
#define MAX_NUMBER_POINT_LIGHTS 0
#define MAX_NUMBER_SPOT_LIGHTS 1
#define MAX_NUMBER_CAPSULE_LIGHTS 0

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

namespace Light
{
    struct HemisphericAmbiental
    {
            XMFLOAT3 downColour;
        private:
            float padding0;
        public:
            XMFLOAT3 colourDifference;
        private:
            float padding1;
    };

    struct Directional
    {
            XMFLOAT3 colour;
        private:
            float padding0;
        public:
            XMFLOAT3 normalizedInvertedDirection;
        private:
            float padding1;
    };

    struct Point
    {
            XMFLOAT3 colour;
            float rangeReciprocal;
            XMFLOAT3 position;
        private:
            float padding;
    };

    struct Spot
    {
        XMFLOAT3 colour;
        float rangeReciprocal;
        XMFLOAT3 position;
        float cosOuterCone;
        XMFLOAT3 normalizedInvertedDirection;
        float cosInnerConeReciprocal;
    };

    struct Capsule
    {
            XMFLOAT3 colour;
            float rangeReciprocal;
            XMFLOAT3 segmentStartPosition;
            float segmentLength;
            XMFLOAT3 normalizedSegmentStartToSegmentEnd;
        private:
            float padding;
    };
}

struct Empty{};

template<typename T, bool empty>
using PotentiallyEmpty = std::conditional_t<empty, T, Empty>;

template<typename T, unsigned short amount>
using PotentiallyEmptyArray = std::conditional_t<std::greater()(amount, 0), std::array<T, amount>, Empty>;

template<typename Data, std::size_t allignment>
struct AlignedBuffer
{
        Data data;
    private:
        float padding[(allignment - (sizeof(Data) % allignment)) / sizeof(float)];
};


namespace ConstantBuffer
{
    template<typename T>
    using Data = AlignedBuffer<T, 256>;

    struct PerModel
    {
            XMMATRIX model;
            XMMATRIX textureTransform;
            XMFLOAT4 diffuseColour;
            float specularExponent;
            float specularIntensity;
        private:
            XMFLOAT2 padding;
    };

    template<bool useHemisphericAmbientalLighting, unsigned short directionalLightsCount, 
        unsigned short pointLightsCount, unsigned short spotLightsCount, unsigned short capsuleLightsCount>
    struct PerScene
    {
            XMMATRIX viewProjection;
            XMFLOAT3 cameraPosition;
        private:
            float padding;
        public:
#if USE_HEMISPHERIC_AMBIENTAL_LIGHTING
            [[no_unique_address]] PotentiallyEmpty<Light::HemisphericAmbiental, useHemisphericAmbientalLighting> ambientalLight;
#endif
#if MAX_NUMBER_DIRECTIONAL_LIGHTS > 0
            [[no_unique_address]] PotentiallyEmptyArray<Light::Directional, directionalLightsCount> directionalLights;
#endif
#if MAX_NUMBER_POINT_LIGHTS > 0
            [[no_unique_address]] PotentiallyEmptyArray<Light::Point, pointLightsCount> pointLights;
#endif
#if MAX_NUMBER_SPOT_LIGHTS > 0
            [[no_unique_address]] PotentiallyEmptyArray<Light::Spot, spotLightsCount> spotLights;
#endif
#if MAX_NUMBER_CAPSULE_LIGHTS > 0
            [[no_unique_address]] PotentiallyEmptyArray<Light::Capsule, capsuleLightsCount> capsuleLights;
#endif
    };

};

template<typename T>
struct WriteBuffer
{
    T data;
    ComPtr<ID3D12Resource> dataGPU;
    void* dataCPU;
    void Update()
    {
        memcpy(dataCPU, &data, sizeof(data));
    }
};

struct PositionNormalUV
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
};

enum class MeshType
{
    Grid, Parallelepiped, Skull
};

struct Mesh
{
    ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    UINT indexCount;
};

struct MeshData
{
    std::vector<PositionNormalUV> vertices;
    std::vector<UINT> indices;
};

enum class RenderLayer
{
    Opaque, ChannelStencilWritter, ChannelStencilReader, Transparent
};

struct Model
{
    RenderLayer renderLayer;
    Mesh const* mesh;
    WriteBuffer<ConstantBuffer::Data<ConstantBuffer::PerModel>> buffer;
};

template<size_t sourceCount, size_t... vectorSizeInitialisers>
constexpr std::array<size_t, sizeof... (vectorSizeInitialisers)> Organise()
{
    static_assert((vectorSizeInitialisers + ...) == sourceCount);
    return { vectorSizeInitialisers... };
}

constexpr UINT frameCount = 2;
constexpr size_t meshCount = 3;
constexpr size_t renderLayerCount = 3;
constexpr std::array<size_t, meshCount> modelsPerMesh{ 6, 4, 0 };
constexpr size_t modelCount = std::accumulate(modelsPerMesh.begin(), modelsPerMesh.end(), 0);
constexpr std::array<size_t, renderLayerCount> modelsPerRenderLayer = Organise<modelCount, 1, 3, 6>();
constexpr size_t textureCount = 3;

class D3D12HelloProject : public DXSample
{
public:
    D3D12HelloProject(UINT width, UINT height, std::wstring name);

    virtual void OnInit() final;
    virtual void OnUpdate() final;
    virtual void OnRender() final;
    virtual void OnDestroy() final;
    virtual void OnMouseDown(WPARAM btnState, int x, int y) final;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) final;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) final;

private:
    using PerScene = ConstantBuffer::Data<ConstantBuffer::PerScene<
        USE_HEMISPHERIC_AMBIENTAL_LIGHTING,
        MAX_NUMBER_DIRECTIONAL_LIGHTS,
        MAX_NUMBER_POINT_LIGHTS,
        MAX_NUMBER_SPOT_LIGHTS,
        MAX_NUMBER_CAPSULE_LIGHTS
        >>;  

    // Pipeline objects.
    CD3DX12_VIEWPORT viewport;
    CD3DX12_RECT scissorRect;
    ComPtr<IDXGISwapChain3> swapChain;
    ComPtr<ID3D12Device> device;
    std::array<ComPtr<ID3D12Resource>, frameCount> renderTargets;
    ComPtr<ID3D12Resource> depthStencilBuffer;
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12DescriptorHeap> renderTargetViewHeap;
    ComPtr<ID3D12DescriptorHeap> depthStencilViewHeap;
    ComPtr<ID3D12DescriptorHeap> constantBufferViewHeap;
    ComPtr<ID3D12DescriptorHeap> shaderResourceViewHeap;
    std::array<ComPtr<ID3D12PipelineState>, 4> pipelineStates;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    UINT renderTargetViewDescriptorSize;
    UINT depthStencilViewDescriptorSize;
    UINT shaderBufferResourceViewsDescriptorSize;

    // App resources.
    std::array<Mesh, meshCount> meshes;
    std::array<Model, modelCount> models;
    WriteBuffer<PerScene> perSceneBuffer;
    ComPtr<ID3D12Resource> channelStencilTexture;
    std::array<ComPtr<ID3D12Resource>, textureCount> shaderResourceViewDefaultBuffers;
    std::array<ComPtr<ID3D12Resource>, textureCount> shaderResourceViewUploadBuffers;
    XMFLOAT2 lastMousePosition;
    XMFLOAT3 cameraUp, cameraForward, cameraRight;

    // Synchronization objects.
    UINT frameIndex;
    HANDLE fenceEvent;
    ComPtr<ID3D12Fence> fence;
    UINT64 fenceValue;

    void LoadPipeline();
    void LoadAssets();
    void CreateParallelepiped(float width, float height, float depth, MeshData& parallelepiped);
    void CreateGrid(float width, float depth, UINT vertexColumnCount, UINT vertexRowsCount, MeshData& grid);
    void CreateMesh(const MeshData& data, Mesh& mesh);
    template<typename T>
    void CreateConstantBuffer(CD3DX12_CPU_DESCRIPTOR_HANDLE& descriptorHandle, WriteBuffer<T>& buffer);
    void PopulateCommandList();
    void WaitForPreviousFrame();
};

template<typename T>
void D3D12HelloProject::CreateConstantBuffer(CD3DX12_CPU_DESCRIPTOR_HANDLE& descriptorHandle, WriteBuffer<T>& buffer)
{
    UINT constantBufferSize = sizeof(buffer.data);
    CD3DX12_HEAP_PROPERTIES uploadProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC constantBufferDescription(CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize));
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadProperties,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer.dataGPU)));
    D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDescription = {};
    constantBufferViewDescription.BufferLocation = buffer.dataGPU->GetGPUVirtualAddress();
    constantBufferViewDescription.SizeInBytes = constantBufferSize;
    device->CreateConstantBufferView(&constantBufferViewDescription, descriptorHandle);
    descriptorHandle.Offset(1, shaderBufferResourceViewsDescriptorSize);
    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(buffer.dataGPU->Map(0, &readRange, &buffer.dataCPU));
}
