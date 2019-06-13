#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <opencv2/imgproc.hpp>
#include <iostream>

using namespace cv;
using namespace std;

int main()
{

	std::cout << "Using OpenCV " << CV_MAJOR_VERSION << "." << CV_MINOR_VERSION << "." << CV_SUBMINOR_VERSION << std::endl;

	VideoCapture capture;


	

	capture.open("anil.asf");
	if (!capture.isOpened())  // check if we succeeded
		cout << "err";


	Mat frame, bg, res;
	int res1,res2;
	Mat img_mask;
	Mat blurred;
	int i = 0, j = 0;
	int area, cx, cy;
	string str, count;
	str = "Image";
	int frameno = 0;
	cx = cy = 0;
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	// Mat drawing;
	while (1)
	{
		// Mat z1, z2;

		capture >> frame;


		//frame = cvQueryFrame(capture);
		if (frame.empty())
		{
			printf(" --(!) No captured frame -- Break!");
			break;
		}

		// data7 line(frame,Point(120,140),Point(320,200),Scalar(00, 00, 230),2,8);

		line(frame, Point(90, 155), Point(220, 147), Scalar(00, 00, 230), 1, 8);	//data2

		line(frame, Point(110, 175), Point(240, 167), Scalar(00, 00, 230), 1, 8);	//data2

		if (i == 0)
		{
			bg = frame;
			cvtColor(bg, bg, CV_BGR2GRAY);
			//blur(bg, bg, Size(7,7));

			i = 1;
			//imshow("fore", bg);
		}

		Mat B(frame);
		//imshow("orig", frame);
		cvtColor(frame, frame, CV_BGR2GRAY);
		absdiff(frame, bg, res);
		//imshow("bgs", res);
		threshold(res, res, 20, 255, CV_THRESH_BINARY);

		findContours(res, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

		Mat drawing = Mat::zeros(res.size(), CV_8UC3);


		for (size_t i = 0; i < contours.size(); ++i)
		{


			Rect brect = boundingRect(contours[i]);

			//cout << brect.x<<" "<<brect.y<<"\n";
			cx = brect.x + brect.width / 2;
			cy = brect.y + brect.height / 2;
			res1 =   65*cy + 4* cx - 10000;    //10435
			res2 = 65 * cy + 4 * cx - 11000;    //11815

			//cout << res1<<'\n';
			/*	if (res1 > 0)
			{		//imshow("violate",B)
			rectangle(B, brect, Scalar(200, 100, 50));
			}
			*/

			area = brect.width*brect.height;

			if (area > 50 && frameno == 0){

				if (res1>0 && res2<0)
				{

					//imshow("Frame", drawing);
					count = to_string(j);
					cout << str + count << "\n";
					j++; frameno++;
					rectangle(B, brect, Scalar(200, 100, 50));

					imwrite(str + count + ".jpg", B);
				}
			}
			else
				frameno>15 ? frameno = 0 : frameno++;

			//imshow("violate", B);

			//}

		}
		imshow("rect", B);

		bg = frame;





		if (waitKey(30) >= 0) break;
		//cvWaitKey(33);

	}


	cvWaitKey(0); // Wait for a keystroke in the window
	return 0;

}