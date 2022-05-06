#pragma once

#include "ORBextractor.h"
#include "Frame.h"

class MyPoint
{
public:
    MyPoint(cv::KeyPoint kp)
    {
        angle = kp.angle;
        pt = kp.pt;
    }

    float angle;
    cv::Point2f pt;
};

class FrameHeader
{
public:
    int n_frames;
    double tframe;
    int key_size;
    int des_size;
    int levels[8];
};

class Reply
{
public:
    double tframe;
    double match_rate;
};

class Client
{
public:
    Client()
    {
    }

    virtual ~Client()
    {
        m_fExit = true;
    }

    winrt::Windows::Foundation::IAsyncAction InitializeAsync(std::wstring hostName, std::wstring portName);

    void Signal_for_Save();

    winrt::Windows::Foundation::IAsyncAction StartClient();

    double debug_flag = 0;
    std::string debug_msg = "debug message";

protected:
    void OnFrameArrived(const winrt::Windows::Media::Capture::Frames::MediaFrameReader& sender,
        const winrt::Windows::Media::Capture::Frames::MediaFrameArrivedEventArgs& args);

private:
    static winrt::Windows::Foundation::IAsyncAction receiver(Client* pProcessor);

    void ProcessFrame();

    void ImageConvert(winrt::Windows::Media::Capture::Frames::MediaFrameReference pFrame);

    void SendFrame();

    void Stop();

    long long m_latestTimestamp = 0;

    winrt::Windows::Media::Capture::Frames::MediaFrameReader m_mediaFrameReader = nullptr;
    winrt::event_token m_OnFrameArrivedRegistration;

    ORB_SLAM3::Frame* m_preFrame = nullptr;

    cv::Mat* m_mat = new cv::Mat();
    long long tMat = 0;

    bool m_fExit = false;

    bool m_streamingEnabled = false;

    TimeConverter m_converter;
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_worldCoordSystem = nullptr;

    winrt::Windows::Networking::Sockets::StreamSocket m_streamSocket;
    winrt::Windows::Storage::Streams::DataWriter m_writer;

    std::wstring m_hostName;
    std::wstring m_portName;

    ORB_SLAM3::ORBextractor* m_extractor;

    std::thread* m_tRecv = nullptr;

    uchar fh_buf[sizeof(FrameHeader)];
    uchar rp_buf[sizeof(Reply)];

    static const int IMAGE_HEIGHT = 350;
    static const int IMAGE_WIDTH = 600;

    static const int KBUF_SIZE = 60000;
    static const int DBUF_SIZE = 160000;

    uchar key_buf[KBUF_SIZE] = { 0 };
    uchar k_zipped[KBUF_SIZE] = { 0 };
    uchar des_buf[DBUF_SIZE] = { 0 };
    uchar d_zipped[DBUF_SIZE] = { 0 };
    unsigned long kbuf_index = 0;
    unsigned long ksrc_len = 0;
    unsigned long dbuf_index = 0;
    unsigned long dsrc_len = 0;

    static const int kImageWidth;
    static const wchar_t kSensorName[3];

    bool save_and_stop = false;

    double rate = 0;
};

