#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>


#include <opencv2/imgproc.hpp>

#include "package_bgs/FrameDifferenceBGS.h"


#include "package_bgs/StaticFrameDifferenceBGS.h"
#include <iostream>

using namespace std;
using namespace cv;


/** @function main */
int main(void)
{

	std::cout << "Using OpenCV " << CV_MAJOR_VERSION << "." << CV_MINOR_VERSION << "." << CV_SUBMINOR_VERSION << std::endl;
	
	VideoCapture capture;
	Mat frame;

	

	//-- 2. Read the video stream
	capture.open("dataset/data.avi");
	if (!capture.isOpened()) { printf("--(!)Error opening video \n"); return -1; }

	/* Background Subtraction Algorithm */
	IBGS *bgs;
	//bgs= new FrameDifferenceBGS;

	//bgs = new StaticFrameDifferenceBGS;

	std::cout << "Press 'q' to quit..." << std::endl;
	int key = 0;
	int frameNumber = 1;
	

	while (key != 'q')
	{
		capture.read(frame);
		
		if (frame.empty())
		{
			printf(" --(!) No captured frame -- Break!");
			break;
		}

		Mat img_input(frame);
		imshow("Input", img_input);

		Mat img_mask;
		Mat img_bkgmodel;
		//bgs->process(img_input, img_mask, img_bkgmodel);


		
		key = cvWaitKey(33);
		frameNumber++;
	}

	cvWaitKey(0);
	//delete bgs;

	cvDestroyAllWindows();

	return 0;
}

