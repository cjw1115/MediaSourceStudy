#include "pch.h"
#include "MediaSource.h"

#define CHECK_HR(hr) IF_FAILED_GOTO(hr, done)

#pragma region IMFMediaEventGenerator
HRESULT MediaSource::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    HRESULT hr = m_eventQueue->GetEvent(dwFlags, ppEvent);
    return hr;
}

HRESULT MediaSource::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState)
{
    HRESULT hr = m_eventQueue->BeginGetEvent(pCallback, punkState);
    return hr;
}

HRESULT MediaSource::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    HRESULT hr = m_eventQueue->EndGetEvent(pResult, ppEvent);
    return hr;
}

HRESULT MediaSource::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    HRESULT hr = m_eventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
    return hr;
}
#pragma endregion

HRESULT CreateAudioMediaType(IMFMediaType** ppType)
{
    HRESULT hr = S_OK;
    winrt::com_ptr<IMFMediaType> pType = NULL;

    MPEG1WAVEFORMAT format;
    ZeroMemory(&format, sizeof(format));

    format.wfx.wFormatTag = WAVE_FORMAT_MPEG;
    format.wfx.nChannels = 2;
    format.wfx.nSamplesPerSec = 48000;
    format.wfx.nBlockAlign = 4;
    format.wfx.wBitsPerSample = 0; // Not used.
    format.wfx.cbSize = sizeof(MPEG1WAVEFORMAT) - sizeof(WAVEFORMATEX);
    format.fwHeadLayer = ACM_MPEG_LAYER3;
    format.fwHeadMode = ACM_MPEG_STEREO;
    // Add the "MPEG-1" flag, although it's somewhat redundant.
    format.fwHeadFlags |= ACM_MPEG_ID_MPEG1;

    // Use the structure to initialize the Media Foundation media type.
    CHECK_HR(hr = MFCreateMediaType(pType.put()));
    CHECK_HR(hr = MFInitMediaTypeFromWaveFormatEx(pType.get(), (const WAVEFORMATEX*)&format, sizeof(format)));

    pType.copy_to(ppType);

done:
    return hr;
}

HRESULT CreateVideoMediaType(IMFMediaType** ppType)
{
    HRESULT hr = S_OK;
done:
    return hr;
}

#pragma region IMFMediaSource
HRESULT MediaSource::GetCharacteristics(DWORD* pdwCharacteristics)
{
    EnterCriticalSection(&m_critSec);
    if (pdwCharacteristics == nullptr)
    {
        return E_POINTER;
    }
    HRESULT hr = S_OK;
    *pdwCharacteristics = MFMEDIASOURCE_CAN_PAUSE;
    EnterCriticalSection(&m_critSec);
    return hr;
}

HRESULT MediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor** ppPresentationDescriptor)
{
    HRESULT hr = S_OK;
    winrt::com_ptr<IMFMediaType> audioType, videoType;
    hr = CreateAudioMediaType(audioType.put());
    hr = CreateAudioMediaType(videoType.put());
    std::vector<winrt::com_ptr<IMFMediaType>> types{ videoType,audioType };
    std::vector<IMFStreamDescriptor*> descs{};
    for (size_t i = 0; i < types.size(); i++)
    {
        winrt::com_ptr<IMFStreamDescriptor> desc;
        auto type = types[i].get();
        MFCreateStreamDescriptor(i, 1, &type, desc.put());
        descs.push_back(desc.detach());
    }

    winrt::com_ptr<IMFPresentationDescriptor> presentationDescriptor;
    hr = MFCreatePresentationDescriptor(descs.size(), descs.data(), presentationDescriptor.put());

    DWORD streamsCount;
    hr = presentationDescriptor->GetStreamDescriptorCount(&streamsCount);
    for (size_t i = 0; i < streamsCount; i++)
    {
        hr = presentationDescriptor->SelectStream(i);
    }

    hr = presentationDescriptor->Clone(m_presentationDescriptor.put());
    *ppPresentationDescriptor = presentationDescriptor.detach();

    return hr;
}

HRESULT MediaSource::Start(IMFPresentationDescriptor* pPresentationDescriptor, const GUID* pguidTimeFormat, const PROPVARIANT* pvarStartPosition)
{
    EnterCriticalSection(&m_critSec);
    HRESULT hr = S_OK;
    SourceOp* pAsyncOp = NULL;

    // Check parameters. 
    // Start position and presentation descriptor cannot be NULL.
    if (pvarStartPosition == NULL || pPresentationDescriptor == NULL)
    {
        return E_INVALIDARG;
    }

    // Check the time format. We support only reference time, which is
    // indicated by a NULL parameter or by time format = GUID_NULL.
    if ((pguidTimeFormat != NULL) && (*pguidTimeFormat != GUID_NULL))
    {
        // Unrecognized time format GUID.
        return MF_E_UNSUPPORTED_TIME_FORMAT;
    }

    // Check the data type of the start position.
    if ((pvarStartPosition->vt != VT_I8) && (pvarStartPosition->vt != VT_EMPTY))
    {
        return MF_E_UNSUPPORTED_TIME_FORMAT;
    }

    // Check if this is a seek request. 
    // Currently, this sample does not support seeking.

    if (pvarStartPosition->vt == VT_I8)
    {
        // If the current state is STOPPED, then position 0 is valid.

        // If the current state is anything else, then the 
        // start position must be VT_EMPTY (current position).

        if ((m_state != STATE_STOPPED) || (pvarStartPosition->hVal.QuadPart != 0))
        {
            return MF_E_INVALIDREQUEST;
        }
    }

    // The operation looks OK. Complete the operation asynchronously.

    // Create the state object for the async operation. 
    CHECK_HR(hr = SourceOp::CreateStartOp(pPresentationDescriptor, &pAsyncOp));

    CHECK_HR(hr = pAsyncOp->SetData(*pvarStartPosition));

    // Queue the operation.
    CHECK_HR(hr = QueueOperation(pAsyncOp));

done:
    SAFE_RELEASE(pAsyncOp);
    return hr;
}

HRESULT MediaSource::Stop(void);
HRESULT MediaSource::Pause(void);
HRESULT MediaSource::Shutdown(void);
#pragma endregion

#pragma region Operation Queue
HRESULT MediaSource::ValidateOperation(SourceOp* pOp)
{
    if (m_currentOp != NULL)
    {
        return MF_E_NOTACCEPTING;
    }
    return S_OK;
}

HRESULT MediaSource::DispatchOperation(SourceOp* pOp)
{
    EnterCriticalSection(&m_critSec);
    HRESULT hr = S_OK;
    if (m_state == STATE_SHUTDOWN)
    {
        return S_OK; // Already shut down, ignore the request.
    }
    switch (pOp->Op())
    {
    case SourceOp::OP_START:
        break;
    case SourceOp::OP_STOP:
        break;
    case SourceOp::OP_PAUSE:
        break;
    case SourceOp::OP_REQUEST_DATA:
        break;
    case SourceOp::OP_END_OF_STREAM:
        break;
    default:
        hr = E_UNEXPECTED;
    }
    return hr;
}
#pragma endregion

