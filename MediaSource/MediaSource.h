#pragma once
#include <mfidl.h>
#include <mfapi.h>
#include <Mferror.h>

#include "SourceOp.h"
#include "OpQueue.h"

enum SourceState
{
    STATE_INVALID,      // Initial state. Have not started opening the stream.
    STATE_OPENING,      // BeginOpen is in progress.
    STATE_STOPPED,
    STATE_PAUSED,
    STATE_STARTED,
    STATE_SHUTDOWN
};

class MediaSource :winrt::implements<OpQueue<SourceOp>, IMFMediaSource>
{
public:
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

private:
    CRITICAL_SECTION m_critSec;
    winrt::com_ptr<IMFMediaEventQueue> m_eventQueue;
    winrt::com_ptr<IMFPresentationDescriptor> m_presentationDescriptor;
    SourceState m_state;

    SourceOp* m_currentOp = nullptr;
};

