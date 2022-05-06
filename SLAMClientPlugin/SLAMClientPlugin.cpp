#include "pch.h"
#include "SLAMClientPlugin.h"

void SLAMClient::Initialize()
{
	InitializeVideoFrameProcessorAsync();
}

void SLAMClient::startClient()
{
	m_videoFrameProcessor->StartClient();
}

void SLAMClient::Save_and_Stop()
{
	m_videoFrameProcessor->Signal_for_Save();
}

char* SLAMClient::GetDebugMsg()
{
	return &m_videoFrameProcessor->debug_msg[0];
}

winrt::Windows::Foundation::IAsyncAction SLAMClient::InitializeVideoFrameProcessorAsync()
{
	m_videoFrameProcessor = std::make_unique<Client>();
	if (!m_videoFrameProcessor.get())
	{
		throw winrt::hresult(E_POINTER);
	}

	co_await m_videoFrameProcessor->InitializeAsync(L"192.168.43.211", L"23940");
}