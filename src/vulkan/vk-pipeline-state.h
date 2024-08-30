#pragma once

#include "vk-base.h"

#include <map>
#include <string>

namespace rhi::vk {

class PipelineStateImpl : public PipelineStateBase
{
public:
    PipelineStateImpl(DeviceImpl* device);
    ~PipelineStateImpl();

    // Turns `m_device` into a strong reference.
    // This method should be called before returning the pipeline state object to
    // external users (i.e. via an `IPipelineState` pointer).
    void establishStrongDeviceReference();

    virtual void comFree() override;

    void init(const GraphicsPipelineStateDesc& inDesc);
    void init(const ComputePipelineStateDesc& inDesc);
    void init(const RayTracingPipelineStateDesc& inDesc);

    Result createVKGraphicsPipelineState();

    Result createVKComputePipelineState();

    virtual Result ensureAPIPipelineStateCreated() override;

    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(InteropHandle* outHandle) override;

    BreakableReference<DeviceImpl> m_device;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

class RayTracingPipelineStateImpl : public PipelineStateImpl
{
public:
    std::map<std::string, Index> shaderGroupNameToIndex;
    Int shaderGroupCount;

    RayTracingPipelineStateImpl(DeviceImpl* device);

    uint32_t findEntryPointIndexByName(const std::map<std::string, Index>& entryPointNameToIndex, const char* name);

    Result createVKRayTracingPipelineState();

    virtual Result ensureAPIPipelineStateCreated() override;

    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(InteropHandle* outHandle) override;
};

} // namespace rhi::vk
