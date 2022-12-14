#pragma once
#include <mfidl.h>
enum class Operation
{
    OP_START,
    OP_PAUSE,
    OP_STOP,
    OP_REQUEST_DATA,
    OP_END_OF_STREAM
};

class SourceOp : public winrt::implements<SourceOp, IUnknown>
{
public:
    static HRESULT CreateOp(Operation op, SourceOp** ppOp);
    static HRESULT CreateStartOp(IMFPresentationDescriptor* pPD, SourceOp** ppOp);

    SourceOp(Operation op);
    virtual ~SourceOp();

    HRESULT SetData(const PROPVARIANT& var);

    Operation Op() const { return m_op; }
    const PROPVARIANT& Data() { return m_data; }

protected:
    Operation   m_op;
    PROPVARIANT m_data;     // Data for the operation.
};

class StartOp : public SourceOp
{
public:
    StartOp(IMFPresentationDescriptor* pPD);
    ~StartOp();

    HRESULT GetPresentationDescriptor(IMFPresentationDescriptor** ppPD);

protected:
    winrt::com_ptr<IMFPresentationDescriptor> m_presentationDesc;
};