#include "pch.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Capture::Frames;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;

using namespace cv;

const int Client::kImageWidth = 760;
const wchar_t Client::kSensorName[3] = L"PV";


IAsyncAction Client::InitializeAsync(std::wstring hostName, std::wstring portName)
{
    m_hostName = hostName;
    m_portName = portName;

    winrt::Windows::Foundation::Collections::IVectorView<MediaFrameSourceGroup>
        mediaFrameSourceGroups{ co_await MediaFrameSourceGroup::FindAllAsync() };

    MediaFrameSourceGroup selectedSourceGroup = nullptr;
    MediaCaptureVideoProfile profile = nullptr;
    MediaCaptureVideoProfileMediaDescription desc = nullptr;
    std::vector<MediaFrameSourceInfo> selectedSourceInfos;

    // Find MediaFrameSourceGroup
    for (const MediaFrameSourceGroup& mediaFrameSourceGroup : mediaFrameSourceGroups)
    {
        auto knownProfiles = MediaCapture::FindKnownVideoProfiles(
            mediaFrameSourceGroup.Id(),
            KnownVideoProfile::VideoConferencing);

        for (const auto& knownProfile : knownProfiles)
        {
            for (auto knownDesc : knownProfile.SupportedRecordMediaDescription())
            {
                if ((knownDesc.Width() == kImageWidth)) // && (std::round(knownDesc.FrameRate()) == 15))
                {
                    profile = knownProfile;
                    desc = knownDesc;
                    selectedSourceGroup = mediaFrameSourceGroup;
                    break;
                }
            }
        }
    }

    winrt::check_bool(selectedSourceGroup != nullptr);

    for (auto sourceInfo : selectedSourceGroup.SourceInfos())
    {
        // Workaround since multiple Color sources can be found,
        // and not all of them are necessarily compatible with the selected video profile
        if (sourceInfo.SourceKind() == MediaFrameSourceKind::Color)
        {
            selectedSourceInfos.push_back(sourceInfo);
        }
    }
    winrt::check_bool(!selectedSourceInfos.empty());

    // Initialize a MediaCapture object
    MediaCaptureInitializationSettings settings;
    settings.VideoProfile(profile);
    settings.RecordMediaDescription(desc);
    settings.VideoDeviceId(selectedSourceGroup.Id());
    settings.StreamingCaptureMode(StreamingCaptureMode::Video);
    settings.MemoryPreference(MediaCaptureMemoryPreference::Cpu);
    settings.SharingMode(MediaCaptureSharingMode::ExclusiveControl);
    settings.SourceGroup(selectedSourceGroup);

    MediaCapture mediaCapture = MediaCapture();
    co_await mediaCapture.InitializeAsync(settings);

    MediaFrameSource selectedSource = nullptr;
    MediaFrameFormat preferredFormat = nullptr;

    for (MediaFrameSourceInfo sourceInfo : selectedSourceInfos)
    {
        auto tmpSource = mediaCapture.FrameSources().Lookup(sourceInfo.Id());
        for (MediaFrameFormat format : tmpSource.SupportedFormats())
        {
            if (format.VideoFormat().Width() == kImageWidth)
            {
                selectedSource = tmpSource;
                preferredFormat = format;
                break;
            }
        }
    }

    winrt::check_bool(preferredFormat != nullptr);

    co_await selectedSource.SetFormatAsync(preferredFormat);
    MediaFrameReader mediaFrameReader = co_await mediaCapture.CreateFrameReaderAsync(selectedSource);
    MediaFrameReaderStartStatus status = co_await mediaFrameReader.StartAsync();

    winrt::check_bool(status == MediaFrameReaderStartStatus::Success);

    int nFeatures = 1000;
    int nLevels = 8;
    int fIniThFAST = 20;
    int fMinThFAST = 7;
    float fScaleFactor = 1.2f;
    int threshold = 100;

    m_extractor = new ORB_SLAM3::ORBextractor(5 * nFeatures, fScaleFactor, nLevels, fIniThFAST, fMinThFAST, threshold);

    m_OnFrameArrivedRegistration = mediaFrameReader.FrameArrived({ this, &Client::OnFrameArrived });
}

void Client::OnFrameArrived(
    const MediaFrameReader& sender,
    const MediaFrameArrivedEventArgs& args)
{
    if (MediaFrameReference frame = sender.TryAcquireLatestFrame())
    {
        long long timestamp = m_converter.RelativeTicksToAbsoluteTicks(
            HundredsOfNanoseconds(frame.SystemRelativeTime().Value().count())).count();
        if (timestamp != m_latestTimestamp && m_streamingEnabled)
        {
            m_latestTimestamp = timestamp;
            tMat = timestamp;
            ImageConvert(frame);
            ProcessFrame();
            SendFrame();
        }
    }
}

IAsyncAction Client::StartClient()
{
    if (m_streamingEnabled != true)
    {
        winrt::Windows::Networking::HostName hostName{ m_hostName };
        m_streamSocket = winrt::Windows::Networking::Sockets::StreamSocket();
        co_await m_streamSocket.ConnectAsync(hostName, m_portName);
        m_writer = m_streamSocket.OutputStream();
        m_writer.UnicodeEncoding(UnicodeEncoding::Utf8);
        m_writer.ByteOrder(ByteOrder::LittleEndian);
        m_streamingEnabled = true;

        m_tRecv = new std::thread(receiver, this);
    }
}

IAsyncAction Client::receiver(Client* pProcessor)
{
    DataReader dataReader{ pProcessor->m_streamSocket.InputStream() };
    dataReader.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);

    while (pProcessor->m_streamingEnabled)
    {
        int bytesLoaded = co_await dataReader.LoadAsync(sizeof(Reply));
        dataReader.ReadBytes(pProcessor->rp_buf);
        Reply* reply = (Reply*)pProcessor->rp_buf;
        pProcessor->rate = reply->match_rate;
    }
}

void Client::ProcessFrame(/*cv::Mat im, long long pTimestamp*/)
{
    double tframe = tMat * 1e-7;
    ORB_SLAM3::Frame* preFrame = new ORB_SLAM3::Frame(*m_mat, m_extractor, rate);
    preFrame->timestamp = tframe;

    m_preFrame = preFrame;
}

void Client::ImageConvert(MediaFrameReference pFrame)
{
    SoftwareBitmap softwareBitmap = SoftwareBitmap::Convert(
        pFrame.VideoMediaFrame().SoftwareBitmap(), BitmapPixelFormat::Bgra8);

    int imageWidth = softwareBitmap.PixelWidth();
    int imageHeight = softwareBitmap.PixelHeight();

    BitmapBuffer bitmapBuffer = softwareBitmap.LockBuffer(BitmapBufferAccessMode::Read);

    // Get raw pointer to the buffer object
    uint32_t pixelBufferDataLength = 0;
    uint8_t* pixelBufferData;

    auto spMemoryBufferByteAccess{ bitmapBuffer.CreateReference()
        .as<::Windows::Foundation::IMemoryBufferByteAccess>() };

    spMemoryBufferByteAccess->
        GetBuffer(&pixelBufferData, &pixelBufferDataLength);

    Mat im(imageHeight, imageWidth, CV_8UC4, (void*)pixelBufferData);
    Mat imGray;
    cv::cvtColor(im, imGray, COLOR_BGRA2GRAY);

    cv::resize(imGray, *m_mat, cv::Size(IMAGE_WIDTH, IMAGE_HEIGHT));
}

void Client::SendFrame()
{
    if (save_and_stop)
    {
        Stop();
        return;
    }
    FrameHeader fh;
    fh.n_frames = 1;

    fh.tframe = m_preFrame->timestamp;

    for (int i = 0; i < 8; ++i)
        fh.levels[i] = m_preFrame->vLevels[i];

    int n = m_preFrame->mvKeys.size();

    for (int i = 0; i < n; ++i)
    {
        MyPoint mp(m_preFrame->mvKeys[i]);
        memcpy(key_buf + kbuf_index, &mp, sizeof(MyPoint));
        kbuf_index += sizeof(MyPoint);
    }
    ksrc_len += sizeof(MyPoint) * n;

    Mat des = m_preFrame->mDescriptors;
    memcpy(des_buf + dbuf_index, des.data, des.total());
    dbuf_index += des.total();
    dsrc_len += des.total();

    delete m_preFrame;
    m_preFrame = nullptr;

    unsigned long kzip_len = KBUF_SIZE;
    compress(k_zipped, &kzip_len, key_buf, ksrc_len);
    fh.key_size = kzip_len;

    unsigned long dzip_len = DBUF_SIZE;
    compress(d_zipped, &dzip_len, des_buf, dsrc_len);
    fh.des_size = dzip_len;

    memcpy(fh_buf, &fh, sizeof(FrameHeader));
    m_writer.WriteBytes(fh_buf);

    std::vector<uint8_t> vec_buf{ k_zipped, k_zipped + kzip_len };
    vec_buf.insert(vec_buf.end(), d_zipped, d_zipped + dzip_len);

    m_writer.WriteBytes(vec_buf);

    kbuf_index = 0;
    ksrc_len = 0;
    dbuf_index = 0;
    dsrc_len = 0;

    m_writer.StoreAsync();
}

void Client::Stop() {
    save_and_stop = false;

    FrameHeader fh;
    fh.n_frames = 0;
    fh.tframe = -1.0;
    memcpy(fh_buf, &fh, sizeof(FrameHeader));
    m_writer.WriteBytes(fh_buf);
    m_writer.StoreAsync();

    m_streamingEnabled = false;
}

void Client::Signal_for_Save()
{
    if (m_streamingEnabled == true) {
        save_and_stop = true;
    }
}
