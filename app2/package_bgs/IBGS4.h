#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

class IBGS
{
public:
  virtual void process(const cv::Mat &img_input, cv::Mat &img_output) = 0;
  virtual ~IBGS(){}

private:
  virtual void saveConfig() = 0;
  virtual void loadConfig() = 0;
};