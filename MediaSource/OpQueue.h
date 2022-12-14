#pragma once
#include "AsyncCallback.h"
#include <list>
#include <functional>

template <class OP_TYPE>
class OpQueue : public winrt::implements<OpQueue<OP_TYPE>,IUnknown>
{
public:
    typedef std::list<OP_TYPE*> OperationList;

    OpQueue(CRITICAL_SECTION& critsec
        , std::function<HRESULT(OP_TYPE*)> validateOperation
        , std::function<HRESULT(OP_TYPE*)> dispatchOperation)
        : m_OnProcessQueue(this, &OpQueue::ProcessQueueAsync),
        m_critsec(critsec)
    {
        m_validateOperation = validateOperation;
        m_dispatchOperation = dispatchOperation;
    }

    ~OpQueue() = default;

    HRESULT QueueOperation(OP_TYPE* pOp)
    {
        HRESULT hr = S_OK;
        EnterCriticalSection(&m_critsec);
        m_OpQueue.push_back(pOp);
        pOp->AddRef();
        if (SUCCEEDED(hr))
        {
            hr = ProcessQueue();
        }
        LeaveCriticalSection(&m_critsec);
        return hr;
    }

    HRESULT ProcessQueue()
    {
        HRESULT hr = S_OK;
        if (m_OpQueue.size() > 0)
        {
            hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, &m_OnProcessQueue, NULL);
        }
        return hr;
    }
protected:
    HRESULT ProcessQueueAsync(IMFAsyncResult* /*pResult*/)
    {
        HRESULT hr = S_OK;
        OP_TYPE* pOp = NULL;

        EnterCriticalSection(&m_critsec);

        if (m_OpQueue.size() > 0)
        {
            pOp = m_OpQueue.front();

            
            hr = m_validateOperation(pOp);
            if (SUCCEEDED(hr))
            {
                m_OpQueue.pop_front();
                pOp->Release();
                (void)m_dispatchOperation(pOp);
            }
        }
        LeaveCriticalSection(&m_critsec);
        return hr;
    }

protected:
    OperationList m_OpQueue;
    CRITICAL_SECTION& m_critsec;         // Protects the queue state.
    AsyncCallback<OpQueue>  m_OnProcessQueue;  // ProcessQueueAsync callback.

    std::function<HRESULT(OP_TYPE*)> m_dispatchOperation;
    std::function<HRESULT(OP_TYPE*)> m_validateOperation;
};