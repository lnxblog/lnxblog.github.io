#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "package_bgs/FrameDifferenceBGS.h"

//#include "package_tracking/BlobTracking.h"

#include <opencv2/imgproc.hpp>
 #include <iostream>

using namespace cv;
 using namespace std;

int main()
 {
	 
	std::cout << "Using OpenCV " << CV_MAJOR_VERSION << "." << CV_MINOR_VERSION << "." << CV_SUBMINOR_VERSION << std::endl;


	 //Mat image;
	// image = imread("lena.jpg", CV_LOAD_IMAGE_COLOR); // Read the file
	 //
		//  if (!image.data) // Check for invalid input
		//  {
		//  cout << "Could not open or find the image" << std::endl;
		//  return -1;
		//  }
	 //
		//  namedWindow("Display window", WINDOW_AUTOSIZE);// Create a window for display.
	 //imshow("Display window", image); // Show our image inside it.

	 
	  VideoCapture capture;
	  

	  IBGS *bgs;
	  bgs = new FrameDifferenceBGS;

	  IBGS *bgs1;
	  bgs1 = new FrameDifferenceBGS;

	  /* Blob Tracking Algorithm */
	  Mat img_blob;
	  //BlobTracking* blobTracking;
	  //blobTracking = new BlobTracking;

	  
	  capture.open("./dataset/data.avi");
	  if (!capture.isOpened())  // check if we succeeded
		  cout<<"err";

	  
	 // 
	// 
Mat img_bkgmodel;
			Mat frame;
	  Mat img_mask;
	  Mat blurred;
	  int j = 0;
	  int area;
	  string str,count;
	  str = "Image";
	  int frameno = 0;
	  
	  vector<vector<Point> > contours;
	  vector<Vec4i> hierarchy;
	 // Mat drawing;
	  while (1)
	  { 
		 // Mat z1, z2;
		  
		  capture>>frame;


		  //frame = cvQueryFrame(capture);
		  if (frame.empty())
		  {
			  printf(" --(!) No captured frame -- Break!");
			  break;
		  }

		  Mat img_input(frame);
		 // imshow("Input", img_input);

		 // 
		 // bgs->process(img_input, img_mask, img_bkgmodel);

		 ////if(!img_mask.empty())
		 // // imshow("Foreground", img_mask);

		 // if (!img_mask.empty())
		 // {
			//  // Perform blob tracking
			//  blobTracking->process(img_input, img_mask, img_blob);
		 // }
		 // line(img_input, Point(0, 0), Point(500, 500), Scalar(100, 155, 30));
		 // imshow("drawing", img_input);
		  Mat A = img_input(Range(60, 150), Range(190,320));

		  Mat B(A);

		  bgs1->process(B, img_mask, img_bkgmodel);
		  if (!img_mask.empty())
		  {
			  medianBlur(img_mask, blurred, 7);
			 //imshow("Foreground", A);
		 
			 findContours(blurred, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

			
			
			 

			 Mat drawing = Mat::zeros(blurred.size(), CV_8UC3);
			  for (size_t i = 0; i<contours.size(); ++i)
			  {
				 // cv::drawContours(drawing, contours, i, Scalar(200,100, 0), 1, 8, hierarchy, 0);
				  Rect brect = boundingRect(contours[i]);
				  area= brect.width*brect.height;
				
				  if (area>200 && frameno==0)
				  {
					 // imshow("Frame", drawing);
					  count = to_string(j);
					  cout << str+count<<"\n";
					 //imwrite(str+count+".jpg", A);
					  rectangle(A, brect, Scalar(200,100,50));
					  j++; 
					  frameno++;
				  }
				  else
					  frameno>25?frameno=0:frameno++;

				 
			  }
			 
			  cv::imshow("Frame", A);
		  
		  
			  
		  }
		  
		  





		  if (waitKey(30) >= 0) break;
//cvWaitKey(33);
		
	  }

	 
		 cvWaitKey(0); // Wait for a keystroke in the window
	  return 0;
	 
}