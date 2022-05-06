#pragma once

namespace SLAMClient
{
	extern "C" __declspec(dllexport) void Initialize();

	extern "C" __declspec(dllexport) void startClient();

	extern "C" __declspec(dllexport) void Save_and_Stop();

	extern "C" __declspec(dllexport) char* GetDebugMsg();

	winrt::Windows::Foundation::IAsyncAction
		InitializeVideoFrameProcessorAsync();

	std::unique_ptr<Client> m_videoFrameProcessor = nullptr;
}