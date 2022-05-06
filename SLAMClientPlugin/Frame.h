#pragma once

namespace ORB_SLAM3
{
    class Frame
    {
    public:
        //PreFrame
        Frame(const cv::Mat& im, ORBextractor* extractor, double rate);

        // Extract ORB on the image. 0 for left image and 1 for right image.
        void ExtractORB(int flag, const cv::Mat& im, const int x0, const int x1, double rate);

        std::vector<cv::KeyPoint> mvKeys;

        cv::Mat mDescriptors;

        ORBextractor* mpORBextractorLeft;

        std::vector<int> vLevels;

        int monoLeft;

        double timestamp;
    };

}// namespace ORB_SLAM
