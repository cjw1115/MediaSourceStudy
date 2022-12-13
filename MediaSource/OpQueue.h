#pragma once
#include "AsyncCallback.h"
#include <list>

template <class OP_TYPE>
class OpQueue : public IUnknown
{
public:

    typedef std::list<OP_TYPE> OpList;

    HRESULT QueueOperation(OP_TYPE* pOp)
    {
        HRESULT hr = S_OK;
        EnterCriticalSection(&m_critsec);
        m_OpQueue.push_back(pOp);
        pOp.AddRef();
        if (SUCCEEDED(hr))
        {
            hr = ProcessQueue();
        }
        LeaveCriticalSection(&m_critsec);
        return hr;
    }

protected:

    HRESULT ProcessQueue()
    {
        HRESULT hr = S_OK;
        if (m_OpQueue.size() > 0)
        {
            hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, &m_OnProcessQueue, NULL);
        }
        return hr;
    }

    HRESULT ProcessQueueAsync(IMFAsyncResult* pResult)
    {
        HRESULT hr = S_OK;
        OP_TYPE* pOp = NULL;

        EnterCriticalSection(&m_critsec);

        if (m_OpQueue.size() > 0)
        {
            pOp = m_OpQueue.front();

            hr = ValidateOperation(pOp);
            if (SUCCEEDED(hr))
            {
                m_OpQueue.pop_front();
                pOp.Release();
                (void)DispatchOperation(pOp);
            }
        }
        LeaveCriticalSection(&m_critsec);
        return hr;
    }

    virtual HRESULT DispatchOperation(OP_TYPE* pOp) = 0;
    virtual HRESULT ValidateOperation(OP_TYPE* pOp) = 0;

    OpQueue(CRITICAL_SECTION& critsec)
        : m_OnProcessQueue(this, &OpQueue::ProcessQueueAsync),
        m_critsec(critsec)
    {
    }

    virtual ~OpQueue() {}

protected:
    OpList                  m_OpQueue;         // Queue of operations.
    CRITICAL_SECTION& m_critsec;         // Protects the queue state.
    AsyncCallback<OpQueue>  m_OnProcessQueue;  // ProcessQueueAsync callback.
};