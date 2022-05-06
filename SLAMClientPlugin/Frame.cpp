#include "pch.h"

namespace ORB_SLAM3
{
    Frame::Frame(const cv::Mat& imGray, ORBextractor* extractor, double rate)
    {
        mpORBextractorLeft = extractor;
        ExtractORB(0, imGray, 0, 1000, rate);
    }

    void Frame::ExtractORB(int flag, const cv::Mat& im, const int x0, const int x1, double rate)
    {
        std::vector<int> vLapping = { x0,x1 };
        if (flag == 0)
            monoLeft = (*mpORBextractorLeft)(im, cv::Mat(), mvKeys, mDescriptors, vLapping, vLevels, rate);
    }
} //namespace ORB_SLAM
