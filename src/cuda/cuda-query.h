// cuda-query.h
#pragma once
#include "cuda-base.h"

namespace rhi
{
using namespace Slang;

namespace cuda
{

class QueryPoolImpl : public QueryPoolBase
{
public:
    // The event object for each query. Owned by the pool.
    std::vector<CUevent> m_events;

    // The event that marks the starting point.
    CUevent m_startEvent;

    Result init(const IQueryPool::Desc& desc);

    ~QueryPoolImpl();

    virtual SLANG_NO_THROW Result SLANG_MCALL getResult(
        GfxIndex queryIndex, GfxCount count, uint64_t* data) override;
};

} // namespace cuda
} // namespace rhi
