#include "testing.h"

using namespace rhi;
using namespace rhi::testing;

struct Vertex
{
    float position[3];
};

struct Instance
{
    float position[3];
    float color[3];
};

static const int kVertexCount = 6;
static const Vertex kVertexData[kVertexCount] = {
    // Triangle 1
    {0, 0, 0.5},
    {1, 0, 0.5},
    {0, 1, 0.5},

    // Triangle 2
    {-1, 0, 0.5},
    {0, 0, 0.5},
    {-1, 1, 0.5},
};

static const int kInstanceCount = 2;
static const Instance kInstanceData[kInstanceCount] = {
    {{0, 0, 0}, {1, 0, 0}},
    {{0, -1, 0}, {0, 0, 1}},
};

static const int kIndexCount = 6;
static const uint32_t kIndexData[kIndexCount] = {
    0,
    2,
    5,
    0,
    1,
    2,
};

const int kWidth = 256;
const int kHeight = 256;
const Format format = Format::R32G32B32A32_FLOAT;

static ComPtr<IBuffer> createVertexBuffer(IDevice* device)
{
    BufferDesc vertexBufferDesc;
    vertexBufferDesc.size = kVertexCount * sizeof(Vertex);
    vertexBufferDesc.defaultState = ResourceState::VertexBuffer;
    vertexBufferDesc.allowedStates = ResourceState::VertexBuffer;
    ComPtr<IBuffer> vertexBuffer = device->createBuffer(vertexBufferDesc, &kVertexData[0]);
    REQUIRE(vertexBuffer != nullptr);
    return vertexBuffer;
}

static ComPtr<IBuffer> createInstanceBuffer(IDevice* device)
{
    BufferDesc instanceBufferDesc;
    instanceBufferDesc.size = kInstanceCount * sizeof(Instance);
    instanceBufferDesc.defaultState = ResourceState::VertexBuffer;
    instanceBufferDesc.allowedStates = ResourceState::VertexBuffer;
    ComPtr<IBuffer> instanceBuffer = device->createBuffer(instanceBufferDesc, &kInstanceData[0]);
    REQUIRE(instanceBuffer != nullptr);
    return instanceBuffer;
}

static ComPtr<IBuffer> createIndexBuffer(IDevice* device)
{
    BufferDesc indexBufferDesc;
    indexBufferDesc.size = kIndexCount * sizeof(uint32_t);
    indexBufferDesc.defaultState = ResourceState::IndexBuffer;
    indexBufferDesc.allowedStates = ResourceState::IndexBuffer;
    ComPtr<IBuffer> indexBuffer = device->createBuffer(indexBufferDesc, &kIndexData[0]);
    REQUIRE(indexBuffer != nullptr);
    return indexBuffer;
}

static ComPtr<ITexture> createColorBuffer(IDevice* device)
{
    TextureDesc colorBufferDesc;
    colorBufferDesc.type = TextureType::Texture2D;
    colorBufferDesc.size.width = kWidth;
    colorBufferDesc.size.height = kHeight;
    colorBufferDesc.size.depth = 1;
    colorBufferDesc.numMipLevels = 1;
    colorBufferDesc.format = format;
    colorBufferDesc.defaultState = ResourceState::RenderTarget;
    colorBufferDesc.allowedStates = {ResourceState::RenderTarget, ResourceState::CopySource};
    ComPtr<ITexture> colorBuffer = device->createTexture(colorBufferDesc, nullptr);
    REQUIRE(colorBuffer != nullptr);
    return colorBuffer;
}

class BaseDrawTest
{
public:
    ComPtr<IDevice> device;

    ComPtr<ITransientResourceHeap> transientHeap;
    ComPtr<IPipeline> pipeline;
    ComPtr<IRenderPassLayout> renderPass;
    ComPtr<IFramebuffer> framebuffer;

    ComPtr<IBuffer> vertexBuffer;
    ComPtr<IBuffer> instanceBuffer;
    ComPtr<ITexture> colorBuffer;

    void init(IDevice* device) { this->device = device; }

    void createRequiredResources()
    {
        VertexStreamDesc vertexStreams[] = {
            {sizeof(Vertex), InputSlotClass::PerVertex, 0},
            {sizeof(Instance), InputSlotClass::PerInstance, 1},
        };

        InputElementDesc inputElements[] = {
            // Vertex buffer data
            {"POSITIONA", 0, Format::R32G32B32_FLOAT, offsetof(Vertex, position), 0},

            // Instance buffer data
            {"POSITIONB", 0, Format::R32G32B32_FLOAT, offsetof(Instance, position), 1},
            {"COLOR", 0, Format::R32G32B32_FLOAT, offsetof(Instance, color), 1},
        };
        InputLayoutDesc inputLayoutDesc = {};
        inputLayoutDesc.inputElementCount = SLANG_COUNT_OF(inputElements);
        inputLayoutDesc.inputElements = inputElements;
        inputLayoutDesc.vertexStreamCount = SLANG_COUNT_OF(vertexStreams);
        inputLayoutDesc.vertexStreams = vertexStreams;
        auto inputLayout = device->createInputLayout(inputLayoutDesc);
        REQUIRE(inputLayout != nullptr);

        vertexBuffer = createVertexBuffer(device);
        instanceBuffer = createInstanceBuffer(device);
        colorBuffer = createColorBuffer(device);

        ITransientResourceHeap::Desc transientHeapDesc = {};
        transientHeapDesc.constantBufferSize = 4096;
        REQUIRE_CALL(device->createTransientResourceHeap(transientHeapDesc, transientHeap.writeRef()));

        ComPtr<IShaderProgram> shaderProgram;
        slang::ProgramLayout* slangReflection;
        REQUIRE_CALL(loadGraphicsProgram(
            device,
            shaderProgram,
            "test-instanced-draw",
            "vertexMain",
            "fragmentMain",
            slangReflection
        ));

        FramebufferLayoutDesc framebufferLayoutDesc;
        framebufferLayoutDesc.renderTargetCount = 1;
        framebufferLayoutDesc.renderTargets[0] = {format, 1};
        ComPtr<IFramebufferLayout> framebufferLayout = device->createFramebufferLayout(framebufferLayoutDesc);
        REQUIRE(framebufferLayout != nullptr);

        RenderPipelineDesc pipelineDesc = {};
        pipelineDesc.program = shaderProgram.get();
        pipelineDesc.inputLayout = inputLayout;
        pipelineDesc.framebufferLayout = framebufferLayout;
        pipelineDesc.depthStencil.depthTestEnable = false;
        pipelineDesc.depthStencil.depthWriteEnable = false;
        REQUIRE_CALL(device->createRenderPipeline(pipelineDesc, pipeline.writeRef()));

        IRenderPassLayout::Desc renderPassDesc = {};
        renderPassDesc.framebufferLayout = framebufferLayout;
        renderPassDesc.renderTargetCount = 1;
        IRenderPassLayout::TargetAccessDesc renderTargetAccess = {};
        renderTargetAccess.loadOp = IRenderPassLayout::TargetLoadOp::Clear;
        renderTargetAccess.storeOp = IRenderPassLayout::TargetStoreOp::Store;
        renderTargetAccess.initialState = ResourceState::RenderTarget;
        renderTargetAccess.finalState = ResourceState::CopySource;
        renderPassDesc.renderTargetAccess = &renderTargetAccess;
        REQUIRE_CALL(device->createRenderPassLayout(renderPassDesc, renderPass.writeRef()));

        IResourceView::Desc colorBufferViewDesc;
        memset(&colorBufferViewDesc, 0, sizeof(colorBufferViewDesc));
        colorBufferViewDesc.format = format;
        colorBufferViewDesc.renderTarget.shape = TextureType::Texture2D;
        colorBufferViewDesc.type = IResourceView::Type::RenderTarget;
        auto rtv = device->createTextureView(colorBuffer, colorBufferViewDesc);

        IFramebuffer::Desc framebufferDesc;
        framebufferDesc.renderTargetCount = 1;
        framebufferDesc.depthStencilView = nullptr;
        framebufferDesc.renderTargetViews = rtv.readRef();
        framebufferDesc.layout = framebufferLayout;
        REQUIRE_CALL(device->createFramebuffer(framebufferDesc, framebuffer.writeRef()));
    }

    void checkTestResults(
        int pixelCount,
        int channelCount,
        const int* testXCoords,
        const int* testYCoords,
        float* testResults
    )
    {
        // Read texture values back from four specific pixels located within the triangles
        // and compare against expected values (because testing every single pixel will be too long and tedious
        // and requires maintaining reference images).
        ComPtr<ISlangBlob> resultBlob;
        size_t rowPitch = 0;
        size_t pixelSize = 0;
        REQUIRE_CALL(
            device->readTexture(colorBuffer, ResourceState::CopySource, resultBlob.writeRef(), &rowPitch, &pixelSize)
        );
        auto result = (float*)resultBlob->getBufferPointer();

        int cursor = 0;
        for (int i = 0; i < pixelCount; ++i)
        {
            auto x = testXCoords[i];
            auto y = testYCoords[i];
            auto pixelPtr = result + x * channelCount + y * rowPitch / sizeof(float);
            for (int j = 0; j < channelCount; ++j)
            {
                testResults[cursor] = pixelPtr[j];
                cursor++;
            }
        }

        float expectedResult[] =
            {1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f};
        compareComputeResultFuzzy(testResults, expectedResult, sizeof(expectedResult));
    }
};

struct DrawInstancedTest : BaseDrawTest
{
    void setUpAndDraw()
    {
        createRequiredResources();

        ICommandQueue::Desc queueDesc = {ICommandQueue::QueueType::Graphics};
        auto queue = device->createCommandQueue(queueDesc);
        auto commandBuffer = transientHeap->createCommandBuffer();

        auto encoder = commandBuffer->encodeRenderCommands(renderPass, framebuffer);
        auto rootObject = encoder->bindPipeline(pipeline);

        Viewport viewport = {};
        viewport.maxZ = 1.0f;
        viewport.extentX = kWidth;
        viewport.extentY = kHeight;
        encoder->setViewportAndScissor(viewport);

        uint32_t startVertex = 0;
        uint32_t startInstanceLocation = 0;

        encoder->setVertexBuffer(0, vertexBuffer);
        encoder->setVertexBuffer(1, instanceBuffer);
        encoder->setPrimitiveTopology(PrimitiveTopology::TriangleList);

        encoder->drawInstanced(kVertexCount, kInstanceCount, startVertex, startInstanceLocation);
        encoder->endEncoding();
        commandBuffer->close();
        queue->executeCommandBuffer(commandBuffer);
        queue->waitOnHost();
    }

    void run()
    {
        setUpAndDraw();

        const int kPixelCount = 4;
        const int kChannelCount = 4;
        int testXCoords[kPixelCount] = {64, 192, 64, 192};
        int testYCoords[kPixelCount] = {100, 100, 250, 250};
        float testResults[kPixelCount * kChannelCount];

        checkTestResults(kPixelCount, kChannelCount, testXCoords, testYCoords, testResults);
    }
};

struct DrawIndexedInstancedTest : BaseDrawTest
{
    ComPtr<IBuffer> indexBuffer;

    void setUpAndDraw()
    {
        createRequiredResources();

        ICommandQueue::Desc queueDesc = {ICommandQueue::QueueType::Graphics};
        auto queue = device->createCommandQueue(queueDesc);
        auto commandBuffer = transientHeap->createCommandBuffer();

        auto encoder = commandBuffer->encodeRenderCommands(renderPass, framebuffer);
        auto rootObject = encoder->bindPipeline(pipeline);

        Viewport viewport = {};
        viewport.maxZ = 1.0f;
        viewport.extentX = kWidth;
        viewport.extentY = kHeight;
        encoder->setViewportAndScissor(viewport);

        uint32_t startIndex = 0;
        int32_t startVertex = 0;
        uint32_t startInstanceLocation = 0;

        encoder->setVertexBuffer(0, vertexBuffer);
        encoder->setVertexBuffer(1, instanceBuffer);
        encoder->setIndexBuffer(indexBuffer, Format::R32_UINT);
        encoder->setPrimitiveTopology(PrimitiveTopology::TriangleList);

        encoder->drawIndexedInstanced(kIndexCount, kInstanceCount, startIndex, startVertex, startInstanceLocation);
        encoder->endEncoding();
        commandBuffer->close();
        queue->executeCommandBuffer(commandBuffer);
        queue->waitOnHost();
    }

    void run()
    {
        indexBuffer = createIndexBuffer(device);

        setUpAndDraw();

        const int kPixelCount = 4;
        const int kChannelCount = 4;
        int testXCoords[kPixelCount] = {64, 192, 64, 192};
        int testYCoords[kPixelCount] = {32, 100, 150, 250};
        float testResults[kPixelCount * kChannelCount];

        checkTestResults(kPixelCount, kChannelCount, testXCoords, testYCoords, testResults);
    }
};

struct DrawIndirectTest : BaseDrawTest
{
    ComPtr<IBuffer> indirectBuffer;

    struct IndirectArgData
    {
        float padding; // Ensure args and count don't start at 0 offset for testing purposes
        IndirectDrawArguments args;
    };

    ComPtr<IBuffer> createIndirectBuffer(IDevice* device)
    {
        static const IndirectArgData kIndirectData = {
            42.0f,        // padding
            {6, 2, 0, 0}, // args
        };

        BufferDesc indirectBufferDesc;
        indirectBufferDesc.size = sizeof(IndirectArgData);
        indirectBufferDesc.defaultState = ResourceState::IndirectArgument;
        indirectBufferDesc.allowedStates = ResourceState::IndirectArgument;
        ComPtr<IBuffer> indirectBuffer = device->createBuffer(indirectBufferDesc, &kIndirectData);
        REQUIRE(indirectBuffer != nullptr);
        return indirectBuffer;
    }

    void setUpAndDraw()
    {
        createRequiredResources();

        ICommandQueue::Desc queueDesc = {ICommandQueue::QueueType::Graphics};
        auto queue = device->createCommandQueue(queueDesc);
        auto commandBuffer = transientHeap->createCommandBuffer();

        auto encoder = commandBuffer->encodeRenderCommands(renderPass, framebuffer);
        auto rootObject = encoder->bindPipeline(pipeline);

        Viewport viewport = {};
        viewport.maxZ = 1.0f;
        viewport.extentX = kWidth;
        viewport.extentY = kHeight;
        encoder->setViewportAndScissor(viewport);

        encoder->setVertexBuffer(0, vertexBuffer);
        encoder->setVertexBuffer(1, instanceBuffer);
        encoder->setPrimitiveTopology(PrimitiveTopology::TriangleList);

        uint32_t maxDrawCount = 1;
        Offset argOffset = offsetof(IndirectArgData, args);

        encoder->drawIndirect(maxDrawCount, indirectBuffer, argOffset);
        encoder->endEncoding();
        commandBuffer->close();
        queue->executeCommandBuffer(commandBuffer);
        queue->waitOnHost();
    }

    void run()
    {
        indirectBuffer = createIndirectBuffer(device);

        setUpAndDraw();

        const int kPixelCount = 4;
        const int kChannelCount = 4;
        int testXCoords[kPixelCount] = {64, 192, 64, 192};
        int testYCoords[kPixelCount] = {100, 100, 250, 250};
        float testResults[kPixelCount * kChannelCount];

        checkTestResults(kPixelCount, kChannelCount, testXCoords, testYCoords, testResults);
    }
};

struct DrawIndexedIndirectTest : BaseDrawTest
{
    ComPtr<IBuffer> indexBuffer;
    ComPtr<IBuffer> indirectBuffer;

    struct IndexedIndirectArgData
    {
        float padding; // Ensure args and count don't start at 0 offset for testing purposes
        IndirectDrawIndexedArguments args;
    };

    ComPtr<IBuffer> createIndirectBuffer(IDevice* device)
    {
        static const IndexedIndirectArgData kIndexedIndirectData = {
            42.0f,           // padding
            {6, 2, 0, 0, 0}, // args
        };

        BufferDesc indirectBufferDesc;
        indirectBufferDesc.size = sizeof(IndexedIndirectArgData);
        indirectBufferDesc.defaultState = ResourceState::IndirectArgument;
        indirectBufferDesc.allowedStates = ResourceState::IndirectArgument;
        ComPtr<IBuffer> indexBuffer = device->createBuffer(indirectBufferDesc, &kIndexedIndirectData);
        REQUIRE(indexBuffer != nullptr);
        return indexBuffer;
    }

    void setUpAndDraw()
    {
        createRequiredResources();

        ICommandQueue::Desc queueDesc = {ICommandQueue::QueueType::Graphics};
        auto queue = device->createCommandQueue(queueDesc);
        auto commandBuffer = transientHeap->createCommandBuffer();

        auto encoder = commandBuffer->encodeRenderCommands(renderPass, framebuffer);
        auto rootObject = encoder->bindPipeline(pipeline);

        Viewport viewport = {};
        viewport.maxZ = 1.0f;
        viewport.extentX = kWidth;
        viewport.extentY = kHeight;
        encoder->setViewportAndScissor(viewport);

        encoder->setVertexBuffer(0, vertexBuffer);
        encoder->setVertexBuffer(1, instanceBuffer);
        encoder->setIndexBuffer(indexBuffer, Format::R32_UINT);
        encoder->setPrimitiveTopology(PrimitiveTopology::TriangleList);

        uint32_t maxDrawCount = 1;
        Offset argOffset = offsetof(IndexedIndirectArgData, args);

        encoder->drawIndexedIndirect(maxDrawCount, indirectBuffer, argOffset);
        encoder->endEncoding();
        commandBuffer->close();
        queue->executeCommandBuffer(commandBuffer);
        queue->waitOnHost();
    }

    void run()
    {
        indexBuffer = createIndexBuffer(device);
        indirectBuffer = createIndirectBuffer(device);

        setUpAndDraw();

        const int kPixelCount = 4;
        const int kChannelCount = 4;
        int testXCoords[kPixelCount] = {64, 192, 64, 192};
        int testYCoords[kPixelCount] = {32, 100, 150, 250};
        float testResults[kPixelCount * kChannelCount];

        checkTestResults(kPixelCount, kChannelCount, testXCoords, testYCoords, testResults);
    }
};

template<typename T>
void testDraw(GpuTestContext* ctx, DeviceType deviceType)
{
    ComPtr<IDevice> device = createTestingDevice(ctx, deviceType);
    T test;
    test.init(device);
    test.run();
}

TEST_CASE("draw-instanced")
{
    runGpuTests(
        testDraw<DrawInstancedTest>,
        {
            DeviceType::D3D11,
            DeviceType::D3D12,
            DeviceType::Vulkan,
            DeviceType::Metal,
        }
    );
}

TEST_CASE("draw-indexed-instanced")
{
    runGpuTests(
        testDraw<DrawIndexedInstancedTest>,
        {
            DeviceType::D3D11,
            DeviceType::D3D12,
            DeviceType::Vulkan,
            DeviceType::Metal,
        }
    );
}

TEST_CASE("draw-indirect")
{
    runGpuTests(
        testDraw<DrawIndirectTest>,
        {
            DeviceType::D3D12,
            DeviceType::Vulkan,
        }
    );
}

TEST_CASE("draw-indexed-indirect")
{
    runGpuTests(
        testDraw<DrawIndexedIndirectTest>,
        {
            DeviceType::D3D12,
            DeviceType::Vulkan,
        }
    );
}
