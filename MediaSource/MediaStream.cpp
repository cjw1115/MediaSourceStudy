#include "pch.h"
#include "MediaStream.h"

#pragma region IMFMediaEventGenerator
HRESULT MediaStream::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    HRESULT hr = m_eventQueue->GetEvent(dwFlags, ppEvent);
    return hr;
}

HRESULT MediaStream::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState)
{
    HRESULT hr = m_eventQueue->BeginGetEvent(pCallback, punkState);
    return hr;
}

HRESULT MediaStream::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    HRESULT hr = m_eventQueue->EndGetEvent(pResult, ppEvent);
    return hr;
}

HRESULT MediaStream::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    HRESULT hr = m_eventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
    return hr;
}
#pragma endregion

MediaStream::MediaStream(DWORD streamIndex,MediaSource* pSource)
{
    m_streamIndex = streamIndex;
    m_parentSource.copy_from(pSource);

    MFCreateEventQueue(m_eventQueue.put());
    InitializeCriticalSection(&m_critSec);

    GenerateStreamDescriptor();
}

MediaStream::~MediaStream()
{
}

HRESULT MediaStream::GetMediaType(IMFMediaType** type)
{
    if (type == NULL)
    {
        return E_POINTER;
    }
    //TODO:: Create MediaType here
    return S_OK;
}

HRESULT MediaStream::GenerateStreamDescriptor()
{
    winrt::com_ptr<IMFMediaType> media_type;
    GetMediaType(media_type.put());

    IMFMediaType* mediaTypes = media_type.get();
    MFCreateStreamDescriptor(m_streamIndex, 1, &mediaTypes,m_streamDesc.put());
    return S_OK;
}

HRESULT MediaStream::GetMediaSource(IMFMediaSource** ppMediaSource)
{
    if (ppMediaSource == NULL)
    {
        return E_POINTER;
    }
    if (m_parentSource == NULL)
    {
        return E_UNEXPECTED;
    }
    auto re = m_parentSource.as<IMFMediaSource>();
    re.copy_to(ppMediaSource);
    return S_OK;
}

HRESULT MediaStream::GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor)
{
    if (ppStreamDescriptor == NULL)
    {
        return E_POINTER;
    }
    if (m_streamDesc == NULL)
    {
        return E_UNEXPECTED;
    }
    m_streamDesc.copy_to(ppStreamDescriptor);
    return S_OK;
}

HRESULT MediaStream::RequestSample(IUnknown* pToken)
{
    HRESULT hr = S_OK;
    AutoLock locker(m_critSec);

    if (m_state == SourceState::STATE_STOPPED)
    {
        CHECK_HR(hr = MF_E_INVALIDREQUEST);
    }

    if (!m_active)
    {
        // If the stream is not active, it should not get sample requests. 
        CHECK_HR(hr = MF_E_INVALIDREQUEST);
    }

    // Fail if we reached the end of the stream AND the sample queue is empty,
    if (m_eos && m_samples.empty())
    {
        CHECK_HR(hr = MF_E_END_OF_STREAM);
    }

    winrt::com_ptr<IUnknown> token;
    token.copy_from(pToken);
    m_requests.push(token);

    // Dispatch the request.
    CHECK_HR(hr = DispatchSamples());

    // If there was an error, queue MEError from the source (except after shutdown).
    if (FAILED(hr) && (m_state != SourceState::STATE_SHUTDOWN))
    {
        hr = m_parentSource->QueueEvent(MEError, GUID_NULL, hr, NULL);
    }
    return hr;
}

HRESULT MediaStream::DispatchSamples()
{
    HRESULT hr = S_OK;

    winrt::com_ptr<IMFSample> pSample = NULL;
    winrt::com_ptr<IUnknown> pToken = NULL;

    AutoLock lock(m_critSec);

    if (m_state != SourceState::STATE_STARTED)
    {
        return S_OK;
    }

    // Deliver as many samples as we can.
    while (!m_samples.empty() && !m_requests.empty())
    {
        pSample = m_samples.front();
        m_samples.pop();

        pToken = m_requests.front();
        m_requests.pop();

        if (pToken)
        {
            CHECK_HR(hr = pSample->SetUnknown(MFSampleExtension_Token, pToken.get()));
        }

        CHECK_HR(hr = m_eventQueue->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, pSample.get()));
    }

    if (m_samples.empty() && m_eos)
    {
        // The sample queue is empty AND we have reached the end of the source stream.
        // Notify the pipeline by sending the end-of-stream event.
        CHECK_HR(hr = m_eventQueue->QueueEventParamVar(MEEndOfStream, GUID_NULL, S_OK, NULL));

        // Also notify the source, so that it can send the end-of-presentation event.
        CHECK_HR(hr = m_parentSource->QueueAsyncOperation(Operation::OP_END_OF_STREAM));
    }
    else if ((m_active && !m_eos && (m_samples.size() < SAMPLE_QUEUE)))
    {
        // The sample queue is empty and the request queue is not empty (and we did not
        // reach the end of the stream). Ask the source for more data.
        CHECK_HR(hr = m_parentSource->QueueAsyncOperation(Operation::OP_REQUEST_DATA));
    }

    // If there was an error, queue MEError from the source (except after shutdown).
    if (FAILED(hr) && (m_state != SourceState::STATE_SHUTDOWN))
    {
        m_parentSource->QueueEvent(MEError, GUID_NULL, hr, NULL);
    }
    return S_OK;
}

HRESULT MediaStream::Activate(bool bActive)
{
    AutoLock lock(m_critSec);

    if (bActive == m_active)
    {
        return S_OK; // No op
    }

    m_active = bActive;

    if (!bActive)
    {
        while (m_samples.size() > 0)
        {
            m_samples.pop();
        }
        while (m_requests.size() > 0)
        {
            m_requests.pop();
        }
    }
    return S_OK;
}


HRESULT MediaStream::Start(const PROPVARIANT& varStart)
{
    AutoLock lock(m_critSec);
    HRESULT hr = S_OK;
    // Queue the stream-started event.
    CHECK_HR(hr = QueueEvent(
        MEStreamStarted,
        GUID_NULL,
        S_OK,
        &varStart
    ));

    m_state = SourceState::STATE_STARTED;

    // If we are restarting from paused, there may be 
    // queue sample requests. Dispatch them now.
    CHECK_HR(hr = DispatchSamples());
    return hr;
}