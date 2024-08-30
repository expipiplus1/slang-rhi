#include "simple-render-pass-layout.h"

#include "renderer-shared.h"

namespace rhi {

IRenderPassLayout* SimpleRenderPassLayout::getInterface(const Guid& guid)
{
    if (guid == GUID::IID_ISlangUnknown || guid == GUID::IID_IRenderPassLayout)
        return static_cast<IRenderPassLayout*>(this);
    return nullptr;
}

void SimpleRenderPassLayout::init(const IRenderPassLayout::Desc& desc)
{
    m_renderTargetAccesses.resize(desc.renderTargetCount);
    for (GfxIndex i = 0; i < desc.renderTargetCount; i++)
        m_renderTargetAccesses[i] = desc.renderTargetAccess[i];
    m_hasDepthStencil = (desc.depthStencilAccess != nullptr);
    if (m_hasDepthStencil)
        m_depthStencilAccess = *desc.depthStencilAccess;
}

} // namespace rhi
