#pragma once

#include "vk-base.h"
#include "vk-device.h"

namespace rhi::vk {

class RenderPassLayoutImpl : public IRenderPassLayout, public ComObject
{
public:
    SLANG_COM_OBJECT_IUNKNOWN_ALL
    IRenderPassLayout* getInterface(const Guid& guid);

public:
    VkRenderPass m_renderPass;
    RefPtr<DeviceImpl> m_renderer;
    ~RenderPassLayoutImpl();

    Result init(DeviceImpl* renderer, const IRenderPassLayout::Desc& desc);
};

} // namespace rhi::vk
