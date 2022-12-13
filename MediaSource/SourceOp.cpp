#include "pch.h"
#include "SourceOp.h"
#include <Mferror.h>

SourceOp::SourceOp(Operation op) : m_op(op)
{
    PropVariantInit(&m_data);
}

SourceOp::~SourceOp()
{
    PropVariantClear(&m_data);
}

HRESULT SourceOp::CreateOp(SourceOp::Operation op, SourceOp** ppOp)
{
    if (ppOp == NULL)
    {
        return E_POINTER;
    }

    auto pOp = winrt::make_self<SourceOp>(op);
    if (pOp == NULL)
    {
        return E_OUTOFMEMORY;
    }
    *ppOp = pOp.detach();

    return S_OK;
}

HRESULT SourceOp::CreateStartOp(IMFPresentationDescriptor* pPD, SourceOp** ppOp)
{
    if (ppOp == NULL)
    {
        return E_POINTER;
    }

    auto pOp = winrt::make_self<StartOp>(pPD);
    if (pOp == NULL)
    {
        return E_OUTOFMEMORY;
    }

    *ppOp = pOp.detach();
    return S_OK;
}

HRESULT SourceOp::SetData(const PROPVARIANT& var)
{
    return PropVariantCopy(&m_data, &var);
}

StartOp::StartOp(IMFPresentationDescriptor* pPD) : SourceOp(SourceOp::OP_START)
{
    m_presentationDesc.copy_from(pPD);
}

StartOp::~StartOp() {}


HRESULT StartOp::GetPresentationDescriptor(IMFPresentationDescriptor** ppPD)
{
    if (ppPD == NULL)
    {
        return E_POINTER;
    }
    if (m_presentationDesc == NULL)
    {
        return MF_E_INVALIDREQUEST;
    }
    m_presentationDesc.copy_to(ppPD);
    return S_OK;
}