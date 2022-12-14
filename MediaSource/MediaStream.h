#pragma once
#include <mfidl.h>
#include "MediaSource.h"
#include <queue>

const DWORD SAMPLE_QUEUE = 2;
class MediaSource;

class MediaStream: public winrt::implements<MediaStream, IMFMediaStream>
{
public:
    MediaStream(DWORD streamIndex, MediaSource* pSource);
    ~MediaStream();

    // IMFMediaEventGenerator
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState);
    STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
    STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
    STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue);

    // IMFMediaStream
    STDMETHODIMP GetMediaSource(IMFMediaSource** ppMediaSource);
    STDMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor);
    STDMETHODIMP RequestSample(IUnknown* pToken);

    HRESULT GetMediaType(IMFMediaType** type);
    HRESULT GenerateStreamDescriptor();

    DWORD GetStreamIdentifier() const { return m_streamIndex; }
    bool IsActive() const { return m_active; }
    HRESULT Activate(bool bActive);
    HRESULT Start(const PROPVARIANT& varStart);

protected:
    HRESULT DispatchSamples();

private:
    CRITICAL_SECTION m_critSec;
    winrt::com_ptr<MediaSource> m_parentSource;
    winrt::com_ptr<IMFStreamDescriptor> m_streamDesc;
    winrt::com_ptr<IMFMediaEventQueue> m_eventQueue;
    SourceState m_state = SourceState::STATE_INVALID;
    bool m_active;
    bool m_eos;
    std::queue<winrt::com_ptr<IMFSample>> m_samples;
    std::queue<winrt::com_ptr<IUnknown>> m_requests;
    DWORD m_streamIndex;
};

class AutoLock
{
private:
    LPCRITICAL_SECTION m_critSec;
public:
    AutoLock(CRITICAL_SECTION critSec)
    {
        m_critSec = &critSec;
        EnterCriticalSection(m_critSec);
    }
    ~AutoLock()
    {
        LeaveCriticalSection(m_critSec);
    }
};
