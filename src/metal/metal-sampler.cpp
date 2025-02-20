#include "metal-sampler.h"
#include "metal-util.h"

namespace rhi::metal {

SamplerImpl::~SamplerImpl() {}

Result SamplerImpl::init(DeviceImpl* device, const SamplerDesc& desc)
{
    m_device = device;

    NS::SharedPtr<MTL::SamplerDescriptor> samplerDesc = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());

    samplerDesc->setMinFilter(MetalUtil::translateSamplerMinMagFilter(desc.minFilter));
    samplerDesc->setMagFilter(MetalUtil::translateSamplerMinMagFilter(desc.magFilter));
    samplerDesc->setMipFilter(MetalUtil::translateSamplerMipFilter(desc.mipFilter));

    samplerDesc->setSAddressMode(MetalUtil::translateSamplerAddressMode(desc.addressU));
    samplerDesc->setTAddressMode(MetalUtil::translateSamplerAddressMode(desc.addressV));
    samplerDesc->setRAddressMode(MetalUtil::translateSamplerAddressMode(desc.addressW));

    samplerDesc->setMaxAnisotropy(std::clamp(desc.maxAnisotropy, 1u, 16u));

    // TODO: support translation of border color...
    MTL::SamplerBorderColor borderColor = MTL::SamplerBorderColorOpaqueBlack;
    samplerDesc->setBorderColor(borderColor);

    samplerDesc->setNormalizedCoordinates(true);

    samplerDesc->setCompareFunction(MetalUtil::translateCompareFunction(desc.comparisonFunc));
    samplerDesc->setLodMinClamp(std::clamp(desc.minLOD, 0.f, 1000.f));
    samplerDesc->setLodMaxClamp(std::clamp(desc.maxLOD, samplerDesc->lodMinClamp(), 1000.f));

    samplerDesc->setSupportArgumentBuffers(true);

    // TODO: no support for reduction op

    m_samplerState = NS::TransferPtr(m_device->m_device->newSamplerState(samplerDesc.get()));

    return m_samplerState ? SLANG_OK : SLANG_FAIL;
}

Result SamplerImpl::getNativeHandle(NativeHandle* outHandle)
{
    outHandle->type = NativeHandleType::MTLSamplerState;
    outHandle->value = (uint64_t)(m_samplerState.get());
    return SLANG_OK;
}

} // namespace rhi::metal
