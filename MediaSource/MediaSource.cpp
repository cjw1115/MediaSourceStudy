#include "pch.h"
#include "MediaSource.h"

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

#pragma region IMFMediaSource
HRESULT MediaSource::GetCharacteristics(DWORD* pdwCharacteristics)
{
    EnterCriticalSection(&m_critSec);
    if (pdwCharacteristics == nullptr)
    {
        return E_POINTER;
    }
    HRESULT hr = S_OK;
    *pdwCharacteristics = MFMEDIASOURCE_CAN_PAUSE | MFMEDIASOURCE_IS_LIVE;
    EnterCriticalSection(&m_critSec);
    return hr;
}

HRESULT MediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor** ppPresentationDescriptor)
{
    HRESULT hr = S_OK;

    std::vector<IMFStreamDescriptor*> streamDescriptors{};
    for (size_t i = 0; i < m_streams.size(); i++)
    {
        auto stream = m_streams[i];
        winrt::com_ptr<IMFStreamDescriptor> desc;
        hr = stream->GetStreamDescriptor(desc.put());
        streamDescriptors.push_back(desc.get());
        desc.detach();
    }

    winrt::com_ptr<IMFPresentationDescriptor> presentationDescriptor;
    hr = MFCreatePresentationDescriptor((DWORD)streamDescriptors.size(), streamDescriptors.data(), presentationDescriptor.put());

    DWORD streamsCount;
    hr = presentationDescriptor->GetStreamDescriptorCount(&streamsCount);
    for (size_t i = 0; i < streamsCount; i++)
    {
        hr = presentationDescriptor->SelectStream((DWORD)i);
    }

    hr = presentationDescriptor->Clone(m_presentationDescriptor.put());
    *ppPresentationDescriptor = presentationDescriptor.detach();

    return hr;
}

HRESULT MediaSource::Start(IMFPresentationDescriptor* pPresentationDescriptor, const GUID* pguidTimeFormat, const PROPVARIANT* pvarStartPosition)
{
    EnterCriticalSection(&m_critSec);
    HRESULT hr = S_OK;
    winrt::com_ptr<SourceOp> pAsyncOp = NULL;

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

        if ((m_state != SourceState::STATE_STOPPED) || (pvarStartPosition->hVal.QuadPart != 0))
        {
            return MF_E_INVALIDREQUEST;
        }
    }

    // The operation looks OK. Complete the operation asynchronously.

    // Create the state object for the async operation. 
    hr = SourceOp::CreateStartOp(pPresentationDescriptor, pAsyncOp.put());
    hr = pAsyncOp->SetData(*pvarStartPosition);
    hr = m_operationQueue->QueueOperation(pAsyncOp.get());
    return hr;
}

HRESULT MediaSource::Stop(void) { return S_OK; }
HRESULT MediaSource::Pause(void) { return S_OK; }
HRESULT MediaSource::Shutdown(void) { return S_OK; }
#pragma endregion

#pragma region Operation Queue
HRESULT MediaSource::ValidateOperation(SourceOp* /*pOp*/)
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
    if (m_state == SourceState::STATE_SHUTDOWN)
    {
        return S_OK; // Already shut down, ignore the request.
    }
    switch (pOp->Op())
    {
    case Operation::OP_START:
        hr = DoStart((StartOp*)pOp);
        break;
    case Operation::OP_STOP:
        break;
    case Operation::OP_PAUSE:
        break;
    case Operation::OP_REQUEST_DATA:
        break;
    case Operation::OP_END_OF_STREAM:
        break;
    default:
        hr = E_UNEXPECTED;
    }
    return hr;
}

HRESULT MediaSource::QueueAsyncOperation(Operation OpType)
{
    HRESULT hr = S_OK;
    winrt::com_ptr<SourceOp> pOp;
    hr = SourceOp::CreateOp(OpType, pOp.put());
    hr = m_operationQueue->QueueOperation(pOp.get());
    return hr;
}
#pragma endregion

HRESULT MediaSource::BeginAsyncOp(SourceOp* pOp)
{
    if (pOp == NULL || m_currentOp != NULL)
    {
        assert(FALSE);
        return E_FAIL;
    }
    m_currentOp.copy_from(pOp);
    return S_OK;
}

HRESULT MediaSource::CompleteAsyncOp(SourceOp* pOp)
{
    HRESULT hr = S_OK;
    if (pOp == NULL || m_currentOp == NULL)
    {
        assert(FALSE);
        return E_FAIL;
    }

    if (m_currentOp.get() != pOp)
    {
        assert(FALSE);
        return E_FAIL;
    }

    m_currentOp = nullptr;

    // Process the next operation on the queue.
    hr = m_operationQueue->ProcessQueue();
    return hr;
}

HRESULT MediaSource::DoStart(StartOp* pOp)
{
    assert(pOp->Op() == Operation::OP_START);

    winrt::com_ptr<IMFPresentationDescriptor> pPD;

    HRESULT hr = S_OK;

    hr = BeginAsyncOp(pOp);
    hr = pOp->GetPresentationDescriptor(pPD.put());

    // Because this sample does not support seeking, the start
    // position must be 0 (from stopped) or "current position."

    // If the sample supported seeking, we would need to get the
    // start position from the PROPVARIANT data contained in pOp.

    // Select/deselect streams, based on what the caller set in the PD.
    hr = SelectStreams(pPD.get(), pOp->Data());

    m_state = SourceState::STATE_STARTED;

    // Queue the "started" event. The event data is the start position.
    hr = m_eventQueue->QueueEventParamVar(MESourceStarted, GUID_NULL, S_OK, &pOp->Data());
    if (FAILED(hr))
    {
        (void)m_eventQueue->QueueEventParamVar(MESourceStarted, GUID_NULL, hr, NULL);
    }
    CompleteAsyncOp(pOp);
    return hr;
}

HRESULT MediaSource::SelectStreams(
    IMFPresentationDescriptor* /*pPD*/,   // Presentation descriptor.
    const PROPVARIANT varStart        // New start position.
)
{
    HRESULT hr = S_OK;
    BOOL    fSelected = FALSE;
    BOOL    fWasSelected = FALSE;

    winrt::com_ptr<IMFStreamDescriptor> pSD = NULL;
    winrt::com_ptr<MediaStream> pStream = NULL;

    // Reset the pending EOS count.  
    m_pendingEOS = 0;

    // Loop throught the stream descriptors to find which streams are active.
    for (DWORD i = 0; i < m_streams.size(); i++)
    {
        // Was the stream active already?
        fWasSelected = m_streams[i]->IsActive();

        // Activate or deactivate the stream.
        CHECK_HR(hr = m_streams[i]->Activate(fSelected));

        if (fSelected)
        {
            m_pendingEOS++;

            if (fWasSelected)
            {
                // This stream was previously selected. Queue the "updated stream" event.
                CHECK_HR(hr = m_eventQueue->QueueEventParamUnk(MEUpdatedStream, GUID_NULL, hr, m_streams[i].as<IUnknown>().get()));
            }
            else
            {
                // This stream was not previously selected. Queue the "new stream" event.
                CHECK_HR(hr = m_eventQueue->QueueEventParamUnk(MENewStream, GUID_NULL, hr, m_streams[i].as<IUnknown>().get()));
            }

            // Start the stream. The stream will send the appropriate stream event.
            CHECK_HR(hr = m_streams[i]->Start(varStart));
        }
    }
    return hr;
}


MediaSource::MediaSource(CRITICAL_SECTION& critSec) : m_critSec(critSec)
{
    m_operationQueue = winrt::make_self<OpQueue<SourceOp>>(critSec
        , [this](SourceOp* op)->HRESULT
        {
            return ValidateOperation(op);
        },
        [this](SourceOp* op)->HRESULT
        {
            return DispatchOperation(op);
        });
}

MediaSource::~MediaSource()
{
    DeleteCriticalSection(&m_critSec);
}

void MediaSource::Create(MediaSource** pSource)
{
    CRITICAL_SECTION critSec;
    InitializeCriticalSection(&critSec);
    auto source = winrt::make_self<MediaSource>(critSec);
    source.copy_to(pSource);
}

void MediaSource::Initialize()
{
    DWORD streamIndex = 0;
    auto stream = winrt::make_self<MediaStream>(streamIndex, this);
    m_streams.push_back(stream);
}