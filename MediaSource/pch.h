#pragma once
#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <initguid.h>
enum class SourceState
{
    STATE_INVALID,
    STATE_OPENING,
    STATE_STOPPED,
    STATE_PAUSED,
    STATE_STARTED,
    STATE_SHUTDOWN
};

#define CHECK_HR(hr) if(FAILED(hr)) return hr;