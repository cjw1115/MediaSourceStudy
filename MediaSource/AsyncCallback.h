#pragma once
#include <mfapi.h>
template<class T>
class AsyncCallback : public winrt::implements<AsyncCallback<T>, IMFAsyncCallback>
{
public:
    typedef HRESULT(T::* InvokeFn)(IMFAsyncResult* pAsyncResult);

    AsyncCallback(T* pParent, InvokeFn fn)
    {
        m_pParent = pParent;
        m_pInvokeFn = fn;
    }

    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(DWORD*, DWORD*)
    {
        // Implementation of this method is optional.
        return E_NOTIMPL;
    }

    STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult)
    {
        return (m_pParent->*m_pInvokeFn)(pAsyncResult);
    }

    T* m_pParent;
    InvokeFn m_pInvokeFn;
};