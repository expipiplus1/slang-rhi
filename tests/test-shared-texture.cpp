#include "testing.h"

using namespace rhi;
using namespace rhi::testing;

static void setUpAndRunShader(
    IDevice* device,
    ComPtr<ITexture> tex,
    ComPtr<IResourceView> texView,
    ComPtr<IResourceView> bufferView,
    const char* entryPoint,
    ComPtr<ISampler> sampler = nullptr
)
{
    ComPtr<ITransientResourceHeap> transientHeap;
    ITransientResourceHeap::Desc transientHeapDesc = {};
    transientHeapDesc.constantBufferSize = 4096;
    REQUIRE_CALL(device->createTransientResourceHeap(transientHeapDesc, transientHeap.writeRef()));

    ComPtr<IShaderProgram> shaderProgram;
    slang::ProgramLayout* slangReflection;
    REQUIRE_CALL(loadComputeProgram(device, shaderProgram, "trivial-copy", entryPoint, slangReflection));

    ComputePipelineDesc pipelineDesc = {};
    pipelineDesc.program = shaderProgram.get();
    ComPtr<IPipeline> pipeline;
    REQUIRE_CALL(device->createComputePipeline(pipelineDesc, pipeline.writeRef()));

    // We have done all the set up work, now it is time to start recording a command buffer for
    // GPU execution.
    {
        ICommandQueue::Desc queueDesc = {ICommandQueue::QueueType::Graphics};
        auto queue = device->createCommandQueue(queueDesc);

        auto commandBuffer = transientHeap->createCommandBuffer();
        auto encoder = commandBuffer->encodeComputeCommands();

        auto rootObject = encoder->bindPipeline(pipeline);

        ShaderCursor entryPointCursor(rootObject->getEntryPoint(0)); // get a cursor the the first entry-point.

        auto& desc = *tex->getDesc();
        entryPointCursor["width"].setData(desc.size.width);
        entryPointCursor["height"].setData(desc.size.height);

        // Bind texture view to the entry point
        entryPointCursor["tex"].setResource(texView);

        if (sampler)
            entryPointCursor["sampler"].setSampler(sampler);

        // Bind buffer view to the entry point.
        entryPointCursor["buffer"].setResource(bufferView);

        encoder->dispatchCompute(1, 1, 1);
        encoder->endEncoding();
        commandBuffer->close();
        queue->executeCommandBuffer(commandBuffer);
        queue->waitOnHost();
    }
}

static ComPtr<ITexture> createTexture(IDevice* device, Extents extents, Format format, SubresourceData* initialData)
{
    TextureDesc texDesc = {};
    texDesc.type = TextureType::Texture2D;
    texDesc.numMipLevels = 1;
    texDesc.arraySize = 1;
    texDesc.size = extents;
    texDesc.defaultState = ResourceState::UnorderedAccess;
    texDesc.allowedStates = ResourceStateSet(
        ResourceState::ShaderResource,
        ResourceState::UnorderedAccess,
        ResourceState::CopyDestination,
        ResourceState::CopySource
    );
    texDesc.format = format;
    texDesc.isShared = true;

    ComPtr<ITexture> inTex;
    REQUIRE_CALL(device->createTexture(texDesc, initialData, inTex.writeRef()));
    return inTex;
}

static ComPtr<IResourceView> createTexView(IDevice* device, ComPtr<ITexture> inTexture)
{
    ComPtr<IResourceView> texView;
    IResourceView::Desc texViewDesc = {};
    texViewDesc.type = IResourceView::Type::UnorderedAccess;
    texViewDesc.format = inTexture->getDesc()->format; // TODO: Handle typeless formats - rhiIsTypelessFormat(format) ?
                                                       // convertTypelessFormat(format) : format;
    REQUIRE_CALL(device->createTextureView(inTexture, texViewDesc, texView.writeRef()));
    return texView;
}

template<typename T>
ComPtr<IBuffer> createBuffer(IDevice* device, int size, void* initialData)
{
    BufferDesc bufferDesc = {};
    bufferDesc.size = size * sizeof(T);
    bufferDesc.format = Format::Unknown;
    bufferDesc.elementSize = sizeof(T);
    bufferDesc.allowedStates = ResourceStateSet(
        ResourceState::ShaderResource,
        ResourceState::UnorderedAccess,
        ResourceState::CopyDestination,
        ResourceState::CopySource
    );
    bufferDesc.defaultState = ResourceState::UnorderedAccess;
    bufferDesc.memoryType = MemoryType::DeviceLocal;

    ComPtr<IBuffer> outBuffer;
    REQUIRE_CALL(device->createBuffer(bufferDesc, initialData, outBuffer.writeRef()));
    return outBuffer;
}

static ComPtr<IResourceView> createOutBufferView(IDevice* device, ComPtr<IBuffer> outBuffer)
{
    ComPtr<IResourceView> bufferView;
    IResourceView::Desc viewDesc = {};
    viewDesc.type = IResourceView::Type::UnorderedAccess;
    viewDesc.format = Format::Unknown;
    REQUIRE_CALL(device->createBufferView(outBuffer, nullptr, viewDesc, bufferView.writeRef()));
    return bufferView;
}

template<DeviceType DstDeviceType>
void testSharedTexture(GpuTestContext* ctx, DeviceType deviceType)
{
    ComPtr<IDevice> srcDevice = createTestingDevice(ctx, deviceType);
    ComPtr<IDevice> dstDevice = createTestingDevice(ctx, DstDeviceType);

    SamplerDesc samplerDesc;
    auto sampler = dstDevice->createSampler(samplerDesc);

    float initFloatData[16] = {0.0f};
    auto floatResults = createBuffer<float>(dstDevice, 16, initFloatData);
    auto floatBufferView = createOutBufferView(dstDevice, floatResults);

    uint32_t initUintData[16] = {0u};
    auto uintResults = createBuffer<uint32_t>(dstDevice, 16, initUintData);
    auto uintBufferView = createOutBufferView(dstDevice, uintResults);

    int32_t initIntData[16] = {0};
    auto intResults = createBuffer<uint32_t>(dstDevice, 16, initIntData);
    auto intBufferView = createOutBufferView(dstDevice, intResults);

    Extents size = {};
    size.width = 2;
    size.height = 2;
    size.depth = 1;

    Extents bcSize = {};
    bcSize.width = 4;
    bcSize.height = 4;
    bcSize.depth = 1;

    {
        float texData[] =
            {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f};
        SubresourceData subData = {(void*)texData, 32, 0};

        // Create a shareable texture using srcDevice, get its handle, then create a texture using the handle using
        // dstDevice. Read back the texture and check that its contents are correct.
        auto srcTexture = createTexture(srcDevice, size, Format::R32G32B32A32_FLOAT, &subData);

        NativeHandle sharedHandle;
        REQUIRE_CALL(srcTexture->getSharedHandle(&sharedHandle));
        ComPtr<ITexture> dstTexture;
        size_t sizeInBytes = 0;
        size_t alignment = 0;
        REQUIRE_CALL(srcDevice->getTextureAllocationInfo(*(srcTexture->getDesc()), &sizeInBytes, &alignment));
        REQUIRE_CALL(dstDevice->createTextureFromSharedHandle(
            sharedHandle,
            *(srcTexture->getDesc()),
            sizeInBytes,
            dstTexture.writeRef()
        ));
        // Reading back the buffer from srcDevice to make sure it's been filled in before reading anything back from
        // dstDevice
        // TODO: Implement actual synchronization (and not this hacky solution)
        compareComputeResult(dstDevice, dstTexture, ResourceState::ShaderResource, texData, 32, 2);

        auto texView = createTexView(dstDevice, dstTexture);
        setUpAndRunShader(dstDevice, dstTexture, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            dstDevice,
            floatResults,
            makeArray<
                float>(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f)
        );
    }
}

#if SLANG_WIN64
TEST_CASE("shared-texture-cuda")
{
    runGpuTests(
        testSharedTexture<DeviceType::CUDA>,
        {
            DeviceType::Vulkan,
            DeviceType::D3D12,
        }
    );
}
#endif
