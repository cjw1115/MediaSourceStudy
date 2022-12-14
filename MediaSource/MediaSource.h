#pragma once
#include <mfidl.h>
#include <mfapi.h>
#include <Mferror.h>

#include "SourceOp.h"
#include "OpQueue.h"
#include "MediaStream.h"

class MediaStream;
class MediaSource :public winrt::implements<MediaSource, IMFMediaSource>
{
public:
    static void Create(MediaSource** source);

    MediaSource(CRITICAL_SECTION&);
    ~MediaSource();
    void Initialize();

    // IMFMediaEventGenerator
    HRESULT GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
    HRESULT BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState);
    HRESULT EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
    HRESULT QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue);

    // IMFMediaSource
    HRESULT GetCharacteristics(DWORD* pdwCharacteristics);
    HRESULT CreatePresentationDescriptor(IMFPresentationDescriptor** ppPresentationDescriptor);
    HRESULT Start(IMFPresentationDescriptor* pPresentationDescriptor, const GUID* pguidTimeFormat, const PROPVARIANT* pvarStartPosition);
    HRESULT Stop(void);
    HRESULT Pause(void);
    HRESULT Shutdown(void);

    // OpQueue
    HRESULT DispatchOperation(SourceOp* pOp);
    HRESULT ValidateOperation(SourceOp* pOp);
    HRESULT QueueAsyncOperation(Operation OpType);

protected:
    HRESULT BeginAsyncOp(SourceOp* pOp);
    HRESULT CompleteAsyncOp(SourceOp* pOp);

    HRESULT DoStart(StartOp* pOp);
    HRESULT SelectStreams(IMFPresentationDescriptor* pPD,const PROPVARIANT varStart);

private:
    CRITICAL_SECTION m_critSec;
    winrt::com_ptr<IMFMediaEventQueue> m_eventQueue;
    winrt::com_ptr<IMFPresentationDescriptor> m_presentationDescriptor;
    SourceState m_state;

    winrt::com_ptr<SourceOp> m_currentOp;
    winrt::com_ptr<OpQueue<SourceOp>> m_operationQueue;

    std::vector<winrt::com_ptr<MediaStream>> m_streams;
    DWORD m_pendingEOS = 0;
};

