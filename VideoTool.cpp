#include <sstream>
#include <string>
#include <iostream>
#include <string>
//#include <opencv2\highgui.h>
#include "opencv2/highgui/highgui.hpp"
//#include <opencv2\cv.h>
#include "opencv2/opencv.hpp"
#include "sockets.h"
#include "math.h"

using namespace std;
using namespace cv;

//default capture width and height
const int FRAME_WIDTH = 640;
const int FRAME_HEIGHT = 480;
//max number of objects to be detected in frame
const int MAX_NUM_OBJECTS = 50;
//minimum and maximum object area
const int MIN_OBJECT_AREA = 20 * 20;
const int MAX_OBJECT_AREA = FRAME_HEIGHT*FRAME_WIDTH / 1.5;
//names that will appear at the top of each window
std::string windowName = "Original Image";
std::string windowName1 = "HSV Image";
std::string windowName2 = "Thresholded Image";
std::string windowName3 = "After Morphological Operations";
std::string trackbarWindowName = "Trackbars";

class ColorDetection
{
public:
    int x = 0, y = 0;
    int H_MIN, H_MAX, S_MIN, S_MAX, V_MIN, V_MAX;
    Mat threshold;
    Mat HSV;
    bool trackObjects = true;
    bool useMorphOps = true;
    VideoCapture capture;
    Point p;
    //Matrix to store each frame of the webcam feed
    Mat cameraFeed;
    std::string name;

public:
    ColorDetection(int H_MIN, int H_MAX, int S_MIN, int S_MAX, int V_MIN, int V_MAX, const VideoCapture &capture, const std::string name);
    ~ColorDetection();

    void createTrackbars();
    void drawObject(Mat &frame);
    void morphOps(Mat &thresh);
    void trackFilteredObject(Mat &cameraFeed);
    void trackColor(void);
    void on_trackbar(int, void*);
};

void on_trackbar(int, void*)
{

}

string intToString(int number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}

ColorDetection::ColorDetection(int H_MIN, int H_MAX, int S_MIN, int S_MAX, int V_MIN, int V_MAX, const VideoCapture &capture, std::string name) : H_MIN(
        H_MIN), H_MAX(H_MAX), S_MIN(S_MIN), S_MAX(S_MAX), V_MIN(V_MIN), V_MAX(V_MAX), capture(capture), name(name)
{}

ColorDetection::~ColorDetection() {}

void ColorDetection::createTrackbars() {
	//create window for trackbars

    std::string crtTrackbarWindowName = trackbarWindowName + " " + name;
	namedWindow(crtTrackbarWindowName, 0);
	//create memory to store trackbar name on window
	char TrackbarName[50];
	sprintf(TrackbarName, "H_MIN", H_MIN);
	sprintf(TrackbarName, "H_MAX", H_MAX);
	sprintf(TrackbarName, "S_MIN", S_MIN);
	sprintf(TrackbarName, "S_MAX", S_MAX);
	sprintf(TrackbarName, "V_MIN", V_MIN);
	sprintf(TrackbarName, "V_MAX", V_MAX);
	//create trackbars and insert them into window
	//3 parameters are: the address of the variable that is changing when the trackbar is moved(eg.H_LOW),
	//the max value the trackbar can move (eg. H_HIGH),
	//and the function that is called whenever the trackbar is moved(eg. on_trackbar)
	//                                  ---->    ---->     ---->
	createTrackbar("H_MIN", crtTrackbarWindowName, &H_MIN, H_MAX);
	createTrackbar("H_MAX", crtTrackbarWindowName, &H_MAX, H_MAX);
	createTrackbar("S_MIN", crtTrackbarWindowName, &S_MIN, S_MAX);
	createTrackbar("S_MAX", crtTrackbarWindowName, &S_MAX, S_MAX);
	createTrackbar("V_MIN", crtTrackbarWindowName, &V_MIN, V_MAX);
	createTrackbar("V_MAX", crtTrackbarWindowName, &V_MAX, V_MAX);
}

void ColorDetection::drawObject(Mat &frame) {

	//use some of the openCV drawing functions to draw crosshairs
	//on your tracked image!

	//UPDATE:JUNE 18TH, 2013
	//added 'if' anmorphOpsd 'else' statements to prevent
	//memory errors from writing off the screen (ie. (-25,-25) is not within the window!)

	circle(frame, Point(x, y), 20, Scalar(0, 255, 0), 2);
	if (y - 25 > 0)
		line(frame, Point(x, y), Point(x, y - 25), Scalar(0, 255, 0), 2);
	else line(frame, Point(x, y), Point(x, 0), Scalar(0, 255, 0), 2);
	if (y + 25 < FRAME_HEIGHT)
		line(frame, Point(x, y), Point(x, y + 25), Scalar(0, 255, 0), 2);
	else line(frame, Point(x, y), Point(x, FRAME_HEIGHT), Scalar(0, 255, 0), 2);
	if (x - 25 > 0)
		line(frame, Point(x, y), Point(x - 25, y), Scalar(0, 255, 0), 2);
	else line(frame, Point(x, y), Point(0, y), Scalar(0, 255, 0), 2);
	if (x + 25 < FRAME_WIDTH)
		line(frame, Point(x, y), Point(x + 25, y), Scalar(0, 255, 0), 2);
	else line(frame, Point(x, y), Point(FRAME_WIDTH, y), Scalar(0, 255, 0), 2);

	putText(frame, intToString(x) + "," + intToString(y), Point(x, y + 30), 1, 1, Scalar(0, 255, 0), 2);
}
void ColorDetection::morphOps(Mat &thresh)
{
	//create structuring element that will be used to "dilate" and "erode" image.
	//the element chosen here is a 3px by 3px rectangle

	Mat erodeElement = getStructuringElement(MORPH_RECT, Size(3, 3));
	//dilate with larger element so make sure object is nicely visible
	Mat dilateElement = getStructuringElement(MORPH_RECT, Size(8, 8));

	erode(thresh, thresh, erodeElement);
	erode(thresh, thresh, erodeElement);


	dilate(thresh, thresh, dilateElement);
	dilate(thresh, thresh, dilateElement);
}

void ColorDetection::trackFilteredObject(Mat &cameraFeed) {

	Mat temp;
	threshold.copyTo(temp);
	//these two vectors needed for output of findContours
	vector< vector<Point> > contours;
	vector<Vec4i> hierarchy;
	//find contours of filtered image using openCV findContours function
	findContours(temp, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);
	//use moments method to find our filtered object
	double refArea = 0;
	bool objectFound = false;
	if (hierarchy.size() > 0) {
		int numObjects = hierarchy.size();
		//if number of objects greater than MAX_NUM_OBJECTS we have a noisy filter
		if (numObjects < MAX_NUM_OBJECTS) {
			for (int index = 0; index >= 0; index = hierarchy[index][0]) {

				Moments moment = moments((cv::Mat)contours[index]);
				double area = moment.m00;

				//if the area is less than 20 px by 20px then it is probably just noise
				//if the area is the same as the 3/2 of the image size, probably just a bad filter
				//we only want the object with the largest area so we safe a reference area each
				//i//initial min and max HSV filter values.
				//these will be changed using trackbars iteration and compare it to the area in the next iteration.
				if (area > MIN_OBJECT_AREA && area<MAX_OBJECT_AREA && area>refArea) {
                    this->x = moment.m10 / area;
                    this->y = moment.m01 / area;
					objectFound = true;
					refArea = area;
				}
				else objectFound = false;


			}
			//let user know you found an object
			if (objectFound == true) {
				putText(cameraFeed, "Tracking Object", Point(0, 50), 2, 1, Scalar(0, 255, 0), 2);
				//draw object location on screen
				//cout << x << "," << y;
				drawObject(cameraFeed);
			}
		}
		else putText(cameraFeed, "TOO MUCH NOISE! ADJUST FILTER", Point(0, 50), 1, 2, Scalar(0, 0, 255), 2);
	}
}

void ColorDetection::trackColor(void)
{
    //store image to matrix
    capture.read(cameraFeed);
    //convert frame from BGR to HSV colorspace
    cvtColor(cameraFeed, HSV, COLOR_BGR2HSV);
    //filter HSV image between values and store filtered image to
    //threshold matrix

    inRange(HSV, Scalar(H_MIN, S_MIN, V_MIN), Scalar(H_MAX, S_MAX, V_MAX), threshold);

    //perform morphological operations on thresholded image to eliminate noise
    //and emphasize the filtered object(s)
    if (useMorphOps)
    {
        morphOps(threshold);
    }
    //pass in thresholded frame to our object tracking function
    //this function will return the x and y coordinates of the
    //filtered object
    if (trackObjects)
    {
        trackFilteredObject(cameraFeed);
    }

    //show frames
    //imshow(name + windowName2, threshold);
    imshow(name + windowName, cameraFeed);
    //imshow(name + windowName1, HSV);
    //delay 30ms so that screen can refresh.
    //image will not appear without this waitKey() command
    waitKey(30);
}

int main(int argc, char* argv[])
{

	//start an infinite loop where webcam feed is copied to cameraFeed matrix
	//all of our operations will be performed within this loop

	//connect_connection((char*)"193.226.12.217", 20232);

	//send_connection('l');

	//close_connection();

    VideoCapture capture;
    //open capture object at location zero (default location for webcam)
    capture.open(0/*"rtmp://172.16.254.99/live/nimic"*/);
    //set height and width of capture frame
    capture.set(CV_CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
    capture.set(CV_CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);

    ColorDetection inamic(84, 256, 0, 256, 79, 246, capture, "inamic"); // roz
    ColorDetection eu_corp( 0, 90, 53, 190, 179, 256, capture, "eu"); //galben
    ColorDetection eu_cap(74, 256, 115, 256, 104, 256, capture, "cap"); //albastru

    inamic.createTrackbars();
    eu_corp.createTrackbars();
    eu_cap.createTrackbars();

	while (1)
    {
        inamic.trackColor();
        eu_corp.trackColor();
        eu_cap.trackColor();

        double angle = 90;
        if( eu_corp.x != eu_cap.x )
        {
            angle = atan(((double)eu_corp.y - (double)eu_cap.y)/((double)eu_corp.x - (double)eu_cap.x)) * 57.29;
        }
        std::cout << "Angle: " << angle << std::endl;
    }

	return 0;
}
