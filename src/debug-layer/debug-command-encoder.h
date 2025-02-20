#pragma once

#include "debug-base.h"

namespace rhi::debug {

class DebugCommandEncoder : public ICommandEncoder
{
public:
    DebugCommandBuffer* getCommandBuffer() { return commandBuffer; }
    bool getIsOpen() { return isOpen; }
    virtual ICommandEncoder* getBaseObject() = 0;

    virtual void* getInterface(SlangUUID const& uuid) = 0;
    Result queryInterface(SlangUUID const& uuid, void** outObject)
    {
        if (auto ptr = getInterface(uuid))
        {
            *outObject = ptr;
            return SLANG_OK;
        }
        return SLANG_E_NO_INTERFACE;
    }
    uint32_t addRef() { return 2; }
    uint32_t release() { return 2; }

    virtual SLANG_NO_THROW void SLANG_MCALL
    textureBarrier(GfxCount count, ITexture* const* textures, ResourceState src, ResourceState dst) override;

    virtual SLANG_NO_THROW void SLANG_MCALL textureSubresourceBarrier(
        ITexture* texture,
        SubresourceRange subresourceRange,
        ResourceState src,
        ResourceState dst
    ) override;

    virtual SLANG_NO_THROW void SLANG_MCALL
    bufferBarrier(GfxCount count, IBuffer* const* buffers, ResourceState src, ResourceState dst) override;

    virtual SLANG_NO_THROW void SLANG_MCALL beginDebugEvent(const char* name, float rgbColor[3]) override;
    virtual SLANG_NO_THROW void SLANG_MCALL endDebugEvent() override;

    virtual SLANG_NO_THROW void SLANG_MCALL writeTimestamp(IQueryPool* queryPool, GfxIndex queryIndex) override;


public:
    DebugCommandBuffer* commandBuffer;
    bool isOpen = false;
};

class DebugResourceCommandEncoder : public UnownedDebugObject<IResourceCommandEncoder>, public DebugCommandEncoder
{
public:
    SLANG_RHI_FORWARD_COMMAND_ENCODER_IMPL(DebugCommandEncoder)

    virtual ICommandEncoder* getBaseObject() override { return baseObject; }

    virtual void* getInterface(SlangUUID const& uuid) override
    {
        if (uuid == GUID::IID_ICommandEncoder || uuid == GUID::IID_IResourceCommandEncoder ||
            uuid == ISlangUnknown::getTypeGuid())
        {
            return this;
        }
        return nullptr;
    }

public:
    virtual SLANG_NO_THROW void SLANG_MCALL endEncoding() override;

    virtual SLANG_NO_THROW void SLANG_MCALL
    copyBuffer(IBuffer* dst, Offset dstOffset, IBuffer* src, Offset srcOffset, Size size);

    virtual SLANG_NO_THROW void SLANG_MCALL uploadBufferData(IBuffer* dst, Offset offset, Size size, void* data);

    virtual SLANG_NO_THROW void SLANG_MCALL copyTexture(
        ITexture* dst,
        ResourceState dstState,
        SubresourceRange dstSubresource,
        Offset3D dstOffset,
        ITexture* src,
        ResourceState srcState,
        SubresourceRange srcSubresource,
        Offset3D srcOffset,
        Extents extent
    );

    virtual SLANG_NO_THROW void SLANG_MCALL uploadTextureData(
        ITexture* dst,
        SubresourceRange subResourceRange,
        Offset3D offset,
        Extents extent,
        SubresourceData* subResourceData,
        GfxCount subResourceDataCount
    );

    virtual SLANG_NO_THROW void SLANG_MCALL
    clearResourceView(IResourceView* view, ClearValue* clearValue, ClearResourceViewFlags::Enum flags);

    virtual SLANG_NO_THROW void SLANG_MCALL resolveResource(
        ITexture* source,
        ResourceState sourceState,
        SubresourceRange sourceRange,
        ITexture* dest,
        ResourceState destState,
        SubresourceRange destRange
    );

    virtual SLANG_NO_THROW void SLANG_MCALL copyTextureToBuffer(
        IBuffer* dst,
        Offset dstOffset,
        Size dstSize,
        Size dstRowStride,
        ITexture* src,
        ResourceState srcState,
        SubresourceRange srcSubresource,
        Offset3D srcOffset,
        Extents extent
    );

    virtual SLANG_NO_THROW void SLANG_MCALL
    resolveQuery(IQueryPool* queryPool, GfxIndex index, GfxCount count, IBuffer* buffer, Offset offset);
};

class DebugRenderCommandEncoder : public UnownedDebugObject<IRenderCommandEncoder>, public DebugCommandEncoder
{
public:
    SLANG_RHI_FORWARD_COMMAND_ENCODER_IMPL(DebugCommandEncoder)

    virtual ICommandEncoder* getBaseObject() override { return baseObject; }

    virtual void* getInterface(SlangUUID const& uuid) override
    {
        if (uuid == GUID::IID_ICommandEncoder || uuid == GUID::IID_IRenderCommandEncoder ||
            uuid == ISlangUnknown::getTypeGuid())
        {
            return this;
        }
        return nullptr;
    }

public:
    virtual SLANG_NO_THROW void SLANG_MCALL endEncoding() override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    bindPipeline(IPipeline* state, IShaderObject** outRootShaderObject) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    bindPipelineWithRootObject(IPipeline* state, IShaderObject* rootObject) override;
    virtual SLANG_NO_THROW void SLANG_MCALL setViewports(GfxCount count, const Viewport* viewports) override;
    virtual SLANG_NO_THROW void SLANG_MCALL setScissorRects(GfxCount count, const ScissorRect* scissors) override;
    virtual SLANG_NO_THROW void SLANG_MCALL setPrimitiveTopology(PrimitiveTopology topology) override;
    virtual SLANG_NO_THROW void SLANG_MCALL
    setVertexBuffers(GfxIndex startSlot, GfxCount slotCount, IBuffer* const* buffers, const Offset* offsets) override;
    virtual SLANG_NO_THROW void SLANG_MCALL
    setIndexBuffer(IBuffer* buffer, Format indexFormat, Offset offset = 0) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL draw(GfxCount vertexCount, GfxIndex startVertex = 0) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    drawIndexed(GfxCount indexCount, GfxIndex startIndex = 0, GfxIndex baseVertex = 0) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    drawIndirect(GfxCount maxDrawCount, IBuffer* argBuffer, Offset argOffset, IBuffer* countBuffer, Offset countOffset)
        override;
    virtual SLANG_NO_THROW Result SLANG_MCALL drawIndexedIndirect(
        GfxCount maxDrawCount,
        IBuffer* argBuffer,
        Offset argOffset,
        IBuffer* countBuffer,
        Offset countOffset
    ) override;
    virtual SLANG_NO_THROW void SLANG_MCALL setStencilReference(uint32_t referenceValue) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    setSamplePositions(GfxCount samplesPerPixel, GfxCount pixelCount, const SamplePosition* samplePositions) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    drawInstanced(GfxCount vertexCount, GfxCount instanceCount, GfxIndex startVertex, GfxIndex startInstanceLocation)
        override;

    virtual SLANG_NO_THROW Result SLANG_MCALL drawIndexedInstanced(
        GfxCount indexCount,
        GfxCount instanceCount,
        GfxIndex startIndexLocation,
        GfxIndex baseVertexLocation,
        GfxIndex startInstanceLocation
    ) override;

    virtual SLANG_NO_THROW Result SLANG_MCALL drawMeshTasks(int x, int y, int z) override;
};


class DebugComputeCommandEncoder : public UnownedDebugObject<IComputeCommandEncoder>, public DebugCommandEncoder
{
public:
    SLANG_RHI_FORWARD_COMMAND_ENCODER_IMPL(DebugCommandEncoder)

    virtual ICommandEncoder* getBaseObject() override { return baseObject; }

    virtual void* getInterface(SlangUUID const& uuid) override
    {
        if (uuid == GUID::IID_ICommandEncoder || uuid == GUID::IID_IComputeCommandEncoder ||
            uuid == ISlangUnknown::getTypeGuid())
        {
            return this;
        }
        return nullptr;
    }

public:
    virtual SLANG_NO_THROW void SLANG_MCALL endEncoding() override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    bindPipeline(IPipeline* state, IShaderObject** outRootShaderObject) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    bindPipelineWithRootObject(IPipeline* state, IShaderObject* rootObject) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL dispatchCompute(int x, int y, int z) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL dispatchComputeIndirect(IBuffer* cmdBuffer, Offset offset) override;
};

class DebugRayTracingCommandEncoder : public UnownedDebugObject<IRayTracingCommandEncoder>, public DebugCommandEncoder
{
public:
    SLANG_RHI_FORWARD_COMMAND_ENCODER_IMPL(DebugCommandEncoder)

    virtual ICommandEncoder* getBaseObject() override { return baseObject; }

    virtual void* getInterface(SlangUUID const& uuid) override
    {
        if (uuid == GUID::IID_ICommandEncoder || uuid == GUID::IID_IRayTracingCommandEncoder ||
            uuid == ISlangUnknown::getTypeGuid())
        {
            return this;
        }
        return nullptr;
    }

public:
    virtual SLANG_NO_THROW void SLANG_MCALL endEncoding() override;
    virtual SLANG_NO_THROW void SLANG_MCALL buildAccelerationStructure(
        const IAccelerationStructure::BuildDesc& desc,
        GfxCount propertyQueryCount,
        AccelerationStructureQueryDesc* queryDescs
    ) override;
    virtual SLANG_NO_THROW void SLANG_MCALL copyAccelerationStructure(
        IAccelerationStructure* dest,
        IAccelerationStructure* src,
        AccelerationStructureCopyMode mode
    ) override;
    virtual SLANG_NO_THROW void SLANG_MCALL queryAccelerationStructureProperties(
        GfxCount accelerationStructureCount,
        IAccelerationStructure* const* accelerationStructures,
        GfxCount queryCount,
        AccelerationStructureQueryDesc* queryDescs
    ) override;
    virtual SLANG_NO_THROW void SLANG_MCALL
    serializeAccelerationStructure(DeviceAddress dest, IAccelerationStructure* source) override;
    virtual SLANG_NO_THROW void SLANG_MCALL
    deserializeAccelerationStructure(IAccelerationStructure* dest, DeviceAddress source) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL bindPipeline(IPipeline* state, IShaderObject** outRootObject) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    bindPipelineWithRootObject(IPipeline* state, IShaderObject* rootObject) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL
    dispatchRays(GfxIndex rayGenShaderIndex, IShaderTable* shaderTable, GfxCount width, GfxCount height, GfxCount depth)
        override;
};

} // namespace rhi::debug
