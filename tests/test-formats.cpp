#include "testing.h"

using namespace rhi;
using namespace rhi::testing;

Format convertTypelessFormat(Format format)
{
    switch (format)
    {
    case Format::R32G32B32A32_TYPELESS:
        return Format::R32G32B32A32_FLOAT;
    case Format::R32G32B32_TYPELESS:
        return Format::R32G32B32_FLOAT;
    case Format::R32G32_TYPELESS:
        return Format::R32G32_FLOAT;
    case Format::R32_TYPELESS:
        return Format::R32_FLOAT;
    case Format::R16G16B16A16_TYPELESS:
        return Format::R16G16B16A16_FLOAT;
    case Format::R16G16_TYPELESS:
        return Format::R16G16_FLOAT;
    case Format::R16_TYPELESS:
        return Format::R16_FLOAT;
    case Format::R8G8B8A8_TYPELESS:
        return Format::R8G8B8A8_UNORM;
    case Format::R8G8_TYPELESS:
        return Format::R8G8_UNORM;
    case Format::R8_TYPELESS:
        return Format::R8_UNORM;
    case Format::B8G8R8A8_TYPELESS:
        return Format::B8G8R8A8_UNORM;
    case Format::R10G10B10A2_TYPELESS:
        return Format::R10G10B10A2_UINT;
    default:
        return Format::Unknown;
    }
}

void setUpAndRunTest(
    IDevice* device,
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
    REQUIRE_CALL(loadComputeProgram(device, shaderProgram, "test-formats", entryPoint, slangReflection));

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

        // Bind texture view to the entry point
        entryPointCursor.getPath("tex").setResource(texView);

        if (sampler)
            entryPointCursor.getPath("sampler").setSampler(sampler);

        // Bind buffer view to the entry point.
        entryPointCursor.getPath("buffer").setResource(bufferView);

        encoder->dispatchCompute(1, 1, 1);
        encoder->endEncoding();
        commandBuffer->close();
        queue->executeCommandBuffer(commandBuffer);
        queue->waitOnHost();
    }
}

ComPtr<IResourceView> createTexView(IDevice* device, Extents size, Format format, SubresourceData* data, int mips = 1)
{
    // FormatInfo info;
    // rhiGetFormatInfo(format, &info);
    // printf("format %s\n", info.name);

    TextureDesc texDesc = {};
    texDesc.type = TextureType::Texture2D;
    texDesc.numMipLevels = mips;
    texDesc.arraySize = 1;
    texDesc.size = size;
    texDesc.defaultState = ResourceState::ShaderResource;
    texDesc.format = format;

    ComPtr<ITexture> inTex;
    REQUIRE_CALL(device->createTexture(texDesc, data, inTex.writeRef()));

    ComPtr<IResourceView> texView;
    IResourceView::Desc texViewDesc = {};
    texViewDesc.type = IResourceView::Type::ShaderResource;
    texViewDesc.format = rhiIsTypelessFormat(format) ? convertTypelessFormat(format) : format;
    REQUIRE_CALL(device->createTextureView(inTex, texViewDesc, texView.writeRef()));
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

ComPtr<IResourceView> createBufferView(IDevice* device, ComPtr<IBuffer> outBuffer)
{
    ComPtr<IResourceView> bufferView;
    IResourceView::Desc viewDesc = {};
    viewDesc.type = IResourceView::Type::UnorderedAccess;
    viewDesc.format = Format::Unknown;
    REQUIRE_CALL(device->createBufferView(outBuffer, nullptr, viewDesc, bufferView.writeRef()));
    return bufferView;
}

void testFormats(GpuTestContext* ctx, DeviceType deviceType)
{
    ComPtr<IDevice> device = createTestingDevice(ctx, deviceType);

    bool isSwiftShader = isSwiftShaderDevice(device);

    SamplerDesc samplerDesc;
    auto sampler = device->createSampler(samplerDesc);

    float initFloatData[16] = {0.0f};
    auto floatResults = createBuffer<float>(device, 16, initFloatData);
    auto floatBufferView = createBufferView(device, floatResults);

    uint32_t initUintData[16] = {0u};
    auto uintResults = createBuffer<uint32_t>(device, 16, initUintData);
    auto uintBufferView = createBufferView(device, uintResults);

    int32_t initIntData[16] = {0};
    auto intResults = createBuffer<uint32_t>(device, 16, initIntData);
    auto intBufferView = createBufferView(device, intResults);

    Extents size = {};
    size.width = 2;
    size.height = 2;
    size.depth = 1;

    Extents bcSize = {};
    bcSize.width = 4;
    bcSize.height = 4;
    bcSize.depth = 1;

    // Note: D32_FLOAT and D16_UNORM are not directly tested as they are only used for raster. These
    // are the same as R32_FLOAT and R16_UNORM, respectively, when passed to a shader.
    {
        float texData[] =
            {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f};
        SubresourceData subData = {(void*)texData, 32, 0};

        auto texView = createTexView(device, size, Format::R32G32B32A32_FLOAT, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f}
        );

        texView = createTexView(device, size, Format::R32G32B32A32_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f}
        );
    }

    // Ignore this test since it is not supported by swiftshader and nvidia's driver.
    if (false)
    {
        float texData[] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f};
        SubresourceData subData = {(void*)texData, 24, 0};

        auto texView = createTexView(device, size, Format::R32G32B32_FLOAT, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat3");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f}
        );

        texView = createTexView(device, size, Format::R32G32B32_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat3");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f}
        );
    }

    {
        float texData[] = {1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, size, Format::R32G32_FLOAT, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat2");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f});

        texView = createTexView(device, size, Format::R32G32_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat2");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f});
    }

    {
        float texData[] = {1.0f, 0.0f, 0.5f, 0.25f};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R32_FLOAT, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.5f, 0.25f});

        texView = createTexView(device, size, Format::R32_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.5f, 0.25f});
    }

    {
        uint16_t texData[] =
            {15360u, 0u, 0u, 15360u, 0u, 15360u, 0u, 15360u, 0u, 0u, 15360u, 15360u, 14336u, 14336u, 14336u, 15360u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, size, Format::R16G16B16A16_FLOAT, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f}
        );

        texView = createTexView(device, size, Format::R16G16B16A16_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f}
        );
    }

    {
        uint16_t texData[] = {15360u, 0u, 0u, 15360u, 15360u, 15360u, 14336u, 14336u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R16G16_FLOAT, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat2");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f});

        texView = createTexView(device, size, Format::R16G16_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat2");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f});
    }

    {
        uint16_t texData[] = {15360u, 0u, 14336u, 13312u};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::R16_FLOAT, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.5f, 0.25f});

        texView = createTexView(device, size, Format::R16_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.5f, 0.25f});
    }

    {
        uint32_t texData[] = {255u, 0u, 0u, 255u, 0u, 255u, 0u, 255u, 0u, 0u, 255u, 255u, 127u, 127u, 127u, 255u};
        SubresourceData subData = {(void*)texData, 32, 0};

        auto texView = createTexView(device, size, Format::R32G32B32A32_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint4");
        compareComputeResult(
            device,
            uintResults,
            std::array{255u, 0u, 0u, 255u, 0u, 255u, 0u, 255u, 0u, 0u, 255u, 255u, 127u, 127u, 127u, 255u}
        );
    }

    // Ignore this test since validation layer reports that it is unsupported.
    if (false)
    {
        uint32_t texData[] = {255u, 0u, 0u, 0u, 255u, 0u, 0u, 0u, 255u, 127u, 127u, 127u};
        SubresourceData subData = {(void*)texData, 24, 0};

        auto texView = createTexView(device, size, Format::R32G32B32_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint3");
        compareComputeResult(
            device,
            uintResults,
            std::array{255u, 0u, 0u, 0u, 255u, 0u, 0u, 0u, 255u, 127u, 127u, 127u}
        );
    }

    {
        uint32_t texData[] = {255u, 0u, 0u, 255u, 255u, 255u, 127u, 127u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, size, Format::R32G32_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint2");
        compareComputeResult(device, uintResults, std::array{255u, 0u, 0u, 255u, 255u, 255u, 127u, 127u});
    }

    {
        uint32_t texData[] = {255u, 0u, 127u, 73u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R32_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint");
        compareComputeResult(device, uintResults, std::array{255u, 0u, 127u, 73u});
    }

    {
        uint16_t texData[] = {255u, 0u, 0u, 255u, 0u, 255u, 0u, 255u, 0u, 0u, 255u, 255u, 127u, 127u, 127u, 255u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, size, Format::R16G16B16A16_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint4");
        compareComputeResult(
            device,
            uintResults,
            std::array{255u, 0u, 0u, 255u, 0u, 255u, 0u, 255u, 0u, 0u, 255u, 255u, 127u, 127u, 127u, 255u}
        );
    }

    {
        uint16_t texData[] = {255u, 0u, 0u, 255u, 255u, 255u, 127u, 127u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R16G16_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint2");
        compareComputeResult(device, uintResults, std::array{255u, 0u, 0u, 255u, 255u, 255u, 127u, 127u});
    }

    {
        uint16_t texData[] = {255u, 0u, 127u, 73u};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::R16_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint");
        compareComputeResult(device, uintResults, std::array{255u, 0u, 127u, 73u});
    }

    {
        uint8_t texData[] = {255u, 0u, 0u, 255u, 0u, 255u, 0u, 255u, 0u, 0u, 255u, 255u, 127u, 127u, 127u, 255u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R8G8B8A8_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint4");
        compareComputeResult(
            device,
            uintResults,
            std::array{255u, 0u, 0u, 255u, 0u, 255u, 0u, 255u, 0u, 0u, 255u, 255u, 127u, 127u, 127u, 255u}
        );
    }

    {
        uint8_t texData[] = {255u, 0u, 0u, 255u, 255u, 255u, 127u, 127u};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::R8G8_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint2");
        compareComputeResult(device, uintResults, std::array{255u, 0u, 0u, 255u, 255u, 255u, 127u, 127u});
    }

    {
        uint8_t texData[] = {255u, 0u, 127u, 73u};
        SubresourceData subData = {(void*)texData, 2, 0};

        auto texView = createTexView(device, size, Format::R8_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint");
        compareComputeResult(device, uintResults, std::array{255u, 0u, 127u, 73u});
    }

    {
        int32_t texData[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 127, 127, 127, 255};
        SubresourceData subData = {(void*)texData, 32, 0};

        auto texView = createTexView(device, size, Format::R32G32B32A32_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt4");
        compareComputeResult(
            device,
            intResults,
            std::array{255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 127, 127, 127, 255}
        );
    }

    // Ignore this test on swiftshader. Swiftshader produces unsupported format warnings for this test.
    if (false)
    {
        int32_t texData[] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 127, 127, 127};
        SubresourceData subData = {(void*)texData, 24, 0};

        auto texView = createTexView(device, size, Format::R32G32B32_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt3");
        compareComputeResult(device, intResults, std::array{255, 0, 0, 0, 255, 0, 0, 0, 255, 127, 127, 127});
    }

    {
        int32_t texData[] = {255, 0, 0, 255, 255, 255, 127, 127};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, size, Format::R32G32_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt2");
        compareComputeResult(device, intResults, std::array{255, 0, 0, 255, 255, 255, 127, 127});
    }

    {
        int32_t texData[] = {255, 0, 127, 73};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R32_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt");
        compareComputeResult(device, intResults, std::array{255, 0, 127, 73});
    }

    {
        int16_t texData[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 127, 127, 127, 255};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, size, Format::R16G16B16A16_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt4");
        compareComputeResult(
            device,
            intResults,
            std::array{255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 127, 127, 127, 255}
        );
    }

    {
        int16_t texData[] = {255, 0, 0, 255, 255, 255, 127, 127};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R16G16_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt2");
        compareComputeResult(device, intResults, std::array{255, 0, 0, 255, 255, 255, 127, 127});
    }

    {
        int16_t texData[] = {255, 0, 127, 73};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::R16_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt");
        compareComputeResult(device, intResults, std::array{255, 0, 127, 73});
    }

    {
        int8_t texData[] = {127, 0, 0, 127, 0, 127, 0, 127, 0, 0, 127, 127, 0, 0, 0, 127};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R8G8B8A8_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt4");
        compareComputeResult(
            device,
            intResults,
            std::array{127, 0, 0, 127, 0, 127, 0, 127, 0, 0, 127, 127, 0, 0, 0, 127}
        );
    }

    {
        int8_t texData[] = {127, 0, 0, 127, 127, 127, 73, 73};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::R8G8_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt2");
        compareComputeResult(device, intResults, std::array{127, 0, 0, 127, 127, 127, 73, 73});
    }

    {
        int8_t texData[] = {127, 0, 73, 25};
        SubresourceData subData = {(void*)texData, 2, 0};

        auto texView = createTexView(device, size, Format::R8_SINT, &subData);
        setUpAndRunTest(device, texView, intBufferView, "copyTexInt");
        compareComputeResult(device, intResults, std::array{127, 0, 73, 25});
    }

    {
        uint16_t texData[] =
            {65535u, 0u, 0u, 65535u, 0u, 65535u, 0u, 65535u, 0u, 0u, 65535u, 65535u, 32767u, 32767u, 32767u, 32767u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, size, Format::R16G16B16A16_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                1.0f,
                0.0f,
                0.0f,
                1.0f,
                0.0f,
                1.0f,
                0.0f,
                1.0f,
                0.0f,
                0.0f,
                1.0f,
                1.0f,
                0.499992371f,
                0.499992371f,
                0.499992371f,
                0.499992371f
            }
        );
    }

    {
        uint16_t texData[] = {65535u, 0u, 0u, 65535u, 65535u, 65535u, 32767u, 32767u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R16G16_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat2");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.499992371f, 0.499992371f}
        );
    }

    {
        uint16_t texData[] = {65535u, 0u, 32767u, 16383u};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::R16_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.499992371f, 0.249988556f});
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {0u, 0u, 0u, 255u, 127u, 127u, 127u, 255u, 255u, 255u, 255u, 255u, 0u, 0u, 0u, 0u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R8G8B8A8_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.498039216f,
                0.498039216f,
                0.498039216f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f
            }
        );

        texView = createTexView(device, size, Format::R8G8B8A8_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.498039216f,
                0.498039216f,
                0.498039216f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f
            }
        );

        texView = createTexView(device, size, Format::R8G8B8A8_UNORM_SRGB, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.211914062f,
                0.211914062f,
                0.211914062f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f
            }
        );
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {255u, 0u, 0u, 255u, 255u, 255u, 127u, 127u};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::R8G8_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat2");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.498039216f, 0.498039216f}
        );

        texView = createTexView(device, size, Format::R8G8_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat2");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.498039216f, 0.498039216f}
        );
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {255u, 0u, 127u, 63u};
        SubresourceData subData = {(void*)texData, 2, 0};

        auto texView = createTexView(device, size, Format::R8_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.498039216f, 0.247058824f});

        texView = createTexView(device, size, Format::R8_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.498039216f, 0.247058824f});
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {0u, 0u, 0u, 255u, 127u, 127u, 127u, 255u, 255u, 255u, 255u, 255u, 0u, 0u, 0u, 0u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::B8G8R8A8_TYPELESS, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.498039216f,
                0.498039216f,
                0.498039216f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f
            }
        );

        texView = createTexView(device, size, Format::B8G8R8A8_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.498039216f,
                0.498039216f,
                0.498039216f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f
            }
        );

        texView = createTexView(device, size, Format::B8G8R8A8_UNORM_SRGB, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.211914062f,
                0.211914062f,
                0.211914062f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f
            }
        );
    }

    {
        int16_t texData[] = {32767, 0, 0, 32767, 0, 32767, 0, 32767, 0, 0, 32767, 32767, -32768, -32768, 0, 32767};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, size, Format::R16G16B16A16_SNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f}
        );
    }

    {
        int16_t texData[] = {32767, 0, 0, 32767, 32767, 32767, -32768, -32768};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R16G16_SNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat2");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f});
    }

    {
        int16_t texData[] = {32767, 0, -32768, 0};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::R16_SNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, -1.0f, 0.0f});
    }

    {
        int8_t texData[] = {127, 0, 0, 127, 0, 127, 0, 127, 0, 0, 127, 127, -128, -128, 0, 127};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R8G8B8A8_SNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f}
        );
    }

    {
        int8_t texData[] = {127, 0, 0, 127, 127, 127, -128, -128};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::R8G8_SNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat2");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f});
    }

    {
        int8_t texData[] = {127, 0, -128, 0};
        SubresourceData subData = {(void*)texData, 2, 0};

        auto texView = createTexView(device, size, Format::R8_SNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat");
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, -1.0f, 0.0f});
    }

    // Ignore this test on swiftshader. Swiftshader produces unsupported format warnings for this test.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {15u, 240u, 240u, 240u, 0u, 255u, 119u, 119u};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::B4G4R4A4_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                0.0f,
                0.0f,
                1.0f,
                1.0f,
                0.0f,
                1.0f,
                0.0f,
                1.0f,
                1.0f,
                0.0f,
                0.0f,
                1.0f,
                0.466666669f,
                0.466666669f,
                0.466666669f,
                0.466666669f
            }
        );
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint16_t texData[] = {31u, 2016u, 63488u, 31727u};
        SubresourceData subData = {(void*)texData, 4, 0};

        auto texView = createTexView(device, size, Format::B5G6R5_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat3");
        compareComputeResult(
            device,
            floatResults,
            std::array{0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.482352942f, 0.490196079f, 0.482352942f}
        );

        texView = createTexView(device, size, Format::B5G5R5A1_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                0.0f,
                0.0f,
                1.0f,
                0.0f,
                0.0313725509f,
                1.0f,
                0.0f,
                0.0f,
                0.968627453f,
                0.0f,
                0.0f,
                1.0f,
                0.968627453f,
                1.0f,
                0.482352942f,
                0.0f
            }
        );
    }

    {
        uint32_t texData[] = {2950951416u, 2013265920u, 3086219772u, 3087007228u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R9G9B9E5_SHAREDEXP, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat3");
        compareComputeResult(
            device,
            floatResults,
            std::array{63.0f, 63.0f, 63.0f, 0.0f, 0.0f, 0.0f, 127.0f, 127.0f, 127.0f, 127.0f, 127.5f, 127.75f}
        );
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint32_t texData[] = {4294967295u, 0u, 2683829759u, 1193046471u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R10G10B10A2_TYPELESS, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint4");
        compareComputeResult(
            device,
            uintResults,
            std::array{1023u, 1023u, 1023u, 3u, 0u, 0u, 0u, 0u, 511u, 511u, 511u, 2u, 455u, 796u, 113u, 1u}
        );

        texView = createTexView(device, size, Format::R10G10B10A2_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat4");
        compareComputeResult(
            device,
            floatResults,
            std::array{
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                0.499511242f,
                0.499511242f,
                0.499511242f,
                0.66666668f,
                0.444770277f,
                0.778103590f,
                0.110459432f,
                0.333333343f
            }
        );

        texView = createTexView(device, size, Format::R10G10B10A2_UINT, &subData);
        setUpAndRunTest(device, texView, uintBufferView, "copyTexUint4");
        compareComputeResult(
            device,
            uintResults,
            std::array{1023u, 1023u, 1023u, 3u, 0u, 0u, 0u, 0u, 511u, 511u, 511u, 2u, 455u, 796u, 113u, 1u}
        );
    }

    {
        uint32_t texData[] = {3085827519u, 0u, 2951478655u, 1880884096u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, size, Format::R11G11B10_FLOAT, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "copyTexFloat3");
        compareComputeResult(
            device,
            floatResults,
            std::array{254.0f, 254.0f, 252.0f, 0.0f, 0.0f, 0.0f, 127.0f, 127.0f, 126.0f, 0.5f, 0.5f, 0.5f}
        );
    }

    // These BC1 tests also check that mipmaps are working correctly for compressed formats.
    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {16u, 0u, 0u,  0u, 0u,   0u,   0u,   0u,   16u, 0u, 0u,  0u, 0u, 0u,
                             0u,  0u, 16u, 0u, 0u,   0u,   0u,   0u,   0u,  0u, 16u, 0u, 0u, 0u,
                             0u,  0u, 0u,  0u, 255u, 255u, 255u, 255u, 0u,  0u, 0u,  0u};
        SubresourceData subData[] = {
            SubresourceData{(void*)texData, 16, 32},
            SubresourceData{(void*)(texData + 32), 8, 0}
        };
        Extents size = {};
        size.width = 8;
        size.height = 8;
        size.depth = 1;

        auto texView = createTexView(device, size, Format::BC1_UNORM, subData, 2);
        setUpAndRunTest(device, texView, floatBufferView, "sampleMips", sampler);
        compareComputeResult(device, floatResults, std::array{0.0f, 0.0f, 0.517647088f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});

        texView = createTexView(device, size, Format::BC1_UNORM_SRGB, subData, 2);
        setUpAndRunTest(device, texView, floatBufferView, "sampleMips", sampler);
        compareComputeResult(device, floatResults, std::array{0.0f, 0.0f, 0.230468750f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 16u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, bcSize, Format::BC2_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{0.0f, 0.0f, 0.517647088f, 1.0f});

        texView = createTexView(device, bcSize, Format::BC2_UNORM_SRGB, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{0.0f, 0.0f, 0.230468750f, 1.0f});
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {0u, 255u, 255u, 255u, 255u, 255u, 255u, 255u, 16u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, bcSize, Format::BC3_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{0.0f, 0.0f, 0.517647088f, 1.0f});

        texView = createTexView(device, bcSize, Format::BC3_UNORM_SRGB, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{0.0f, 0.0f, 0.230468750f, 1.0f});
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {127u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        SubresourceData subData = {(void*)texData, 8, 0};

        auto texView = createTexView(device, bcSize, Format::BC4_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{0.498039216f, 0.0f, 0.0f, 1.0f});

        texView = createTexView(device, bcSize, Format::BC4_SNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{1.0f, 0.0f, 0.0f, 1.0f});
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {127u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 127u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, bcSize, Format::BC5_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(
            device,
            floatResults,
            std::array{0.498039216f, 0.498039216f, 0.0f, 1.0f, 0.498039216f, 0.498039216f, 0.0f, 1.0f}
        );

        texView = createTexView(device, bcSize, Format::BC5_SNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f});
    }

    // BC6H_UF16 and BC6H_SF16 are tested separately due to requiring different texture data.
    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {98u, 238u, 232u, 77u, 240u, 66u, 148u, 31u, 124u, 95u, 2u, 224u, 255u, 107u, 77u, 250u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, bcSize, Format::BC6H_UF16, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{0.336669922f, 0.911132812f, 2.13867188f, 1.0f});
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {107u, 238u, 232u, 77u, 240u, 71u, 128u, 127u, 1u, 0u, 255u, 255u, 170u, 218u, 221u, 254u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, bcSize, Format::BC6H_SF16, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{0.336914062f, 0.910644531f, 2.14062500f, 1.0f});
    }

    // Ignore this test on swiftshader. Swiftshader produces different results than expected.
    if (!isSwiftShader)
    {
        uint8_t texData[] = {104u, 0u, 0u, 0u, 64u, 163u, 209u, 104u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        SubresourceData subData = {(void*)texData, 16, 0};

        auto texView = createTexView(device, bcSize, Format::BC7_UNORM, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{0.0f, 0.101960786f, 0.0f, 1.0f});

        texView = createTexView(device, bcSize, Format::BC7_UNORM_SRGB, &subData);
        setUpAndRunTest(device, texView, floatBufferView, "sampleTex", sampler);
        compareComputeResult(device, floatResults, std::array{0.0f, 0.0103149414f, 0.0f, 1.0f});
    }
}

TEST_CASE("formats")
{
    runGpuTests(
        testFormats,
        {
            DeviceType::D3D12,
            DeviceType::Vulkan,
        }
    );
}
