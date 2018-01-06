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
const int MAX_OBJECT_AREA = FRAME_HEIGHT * FRAME_WIDTH / 1.5;
//names that will appear at the top of each window
std::string windowName = "Original Image";
std::string windowName1 = "HSV Image";
std::string windowName2 = "Thresholded Image";
std::string windowName3 = "After Morphological Operations";
std::string trackbarWindowName = "Trackbars";

/*Ring data*/
/*------------------------------------------*/
/*Ring center and radius*/
Point ring_center;
unsigned int ring_radius = 0;
/*Hough Circle Transform thresholds*/
int UPPER_THRES = 170, CENTER_THRES = 100;

void detectRing(Mat cameraFeed);
void createRingTrackbars(void);
/*------------------------------------------*/


/*Camera capture data*/
/*------------------------------------------*/
VideoCapture capture;
/*Matrix to store each frame of the webcam feed*/
Mat cameraFeed;
/*------------------------------------------*/

class ColorDetection {
public:
    int x = 0, y = 0;
    int H_MIN, H_MAX, S_MIN, S_MAX, V_MIN, V_MAX;

    Mat threshold;
    Mat HSV;
    bool trackObjects = true;
    bool useMorphOps = true;

    Point p;

    std::string name;

public:
    ColorDetection(int H_MIN, int H_MAX, int S_MIN, int S_MAX, int V_MIN, int V_MAX, const std::string name);

    ~ColorDetection();

    void createTrackbars(void);

    double static calculateAngle(ColorDetection a, ColorDetection b);

    void drawObject(Mat &frame);

    void morphOps(Mat &thresh);

    void trackFilteredObject(Mat &cameraFeed);

    void trackColor(void);

    void on_trackbar(int, void *);
};

void on_trackbar(int, void *) {

}

string intToString(int number) {
    std::stringstream ss;
    ss << number;
    return ss.str();
}

ColorDetection::ColorDetection(int H_MIN, int H_MAX, int S_MIN, int S_MAX, int V_MIN, int V_MAX, std::string name) : H_MIN(
        H_MIN), H_MAX(H_MAX), S_MIN(S_MIN), S_MAX(S_MAX), V_MIN(V_MIN), V_MAX(V_MAX), name(name) {}

ColorDetection::~ColorDetection() {}


double ColorDetection::calculateAngle(ColorDetection a, ColorDetection b) {
    double angle = 90;
    if (a.x != b.x) {
        angle = atan(((double) b.y - (double) a.y) / ((double) b.x - (double) a.x)) * 57.29;
    }
    if (b.x > a.x) {
        angle += 180;
    } else {
        if (a.y < b.y) {
            angle += 360;
        }
    }
    return angle;
}

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

void ColorDetection::morphOps(Mat &thresh) {
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
    vector<vector<Point> > contours;
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

                Moments moment = moments((cv::Mat) contours[index]);
                double area = moment.m00;

                //if the area is less than 20 px by 20px then it is probably just noise
                //if the area is the same as the 3/2 of the image size, probably just a bad filter
                //we only want the object with the largest area so we safe a reference area each
                //i//initial min and max HSV filter values.
                //these will be changed using trackbars iteration and compare it to the area in the next iteration.
                if (area > MIN_OBJECT_AREA && area < MAX_OBJECT_AREA && area > refArea) {
                    this->x = moment.m10 / area;
                    this->y = moment.m01 / area;
                    objectFound = true;
                    refArea = area;
                } else objectFound = false;


            }
            //let user know you found an object
            if (objectFound == true) {
                putText(cameraFeed, "Tracking Object", Point(0, 50), 2, 1, Scalar(0, 255, 0), 2);
                //draw object location on screen
                //cout << x << "," << y;
                drawObject(cameraFeed);
            }
        } else putText(cameraFeed, "TOO MUCH NOISE! ADJUST FILTER", Point(0, 50), 1, 2, Scalar(0, 0, 255), 2);
    }
}

void ColorDetection::trackColor(void) {
    //store image to matrix
    //capture.read(cameraFeed);
    cameraFeed = imread("./poza.png");
    //convert frame from BGR to HSV colorspace
    cvtColor(cameraFeed, HSV, COLOR_BGR2HSV);
    //filter HSV image between values and store filtered image to
    //threshold matrix

    inRange(HSV, Scalar(H_MIN, S_MIN, V_MIN), Scalar(H_MAX, S_MAX, V_MAX), threshold);

    //perform morphological operations on thresholded image to eliminate noise
    //and emphasize the filtered object(s)
    if (useMorphOps) {
        morphOps(threshold);
    }
    //pass in thresholded frame to our object tracking function
    //this function will return the x and y coordinates of the
    //filtered object
    if (trackObjects) {
        trackFilteredObject(cameraFeed);
    }

    //show frames
    imshow(name + windowName + "th", threshold);
    imshow(name + windowName, cameraFeed);
    //imshow(name + windowName1, HSV);
    //delay 30ms so that screen can refresh.
    //image will not appear without this waitKey() command
    waitKey(30);
}

int main(int argc, char *argv[]) {

    //start an infinite loop where webcam feed is copied to cameraFeed matrix
    //all of our operations will be performed within this loop

    //connect_connection((char*)"193.226.12.217", 20232);

    //send_connection('l');

    //close_connection();

    //open capture object at location zero (default location for webcam)
    capture.open(/*"rtmp://172.16.254.99/live/nimic"*/0);
    //set height and width of capture frame
    capture.set(CV_CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
    capture.set(CV_CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);

    /*inamic.createTrackbars();
    eu_cap.createTrackbars();
    eu_corp.createTrackbars();*/

    /*Detect ring center and radius*/
    capture.read(cameraFeed);
    cameraFeed = imread("./poza.png"); //DEBUG
    createRingTrackbars();
    detectRing(cameraFeed);

    ColorDetection inamic(84, 256, 0, 256, 79, 246, "inamic"); // roz
    ColorDetection eu_corp(0, 90, 53, 190, 179, 256, "eu"); //galben
    ColorDetection eu_cap(74, 256, 115, 256, 104, 256, "cap"); //albastru

    while (1) {
        inamic.trackColor();
        eu_cap.trackColor();
        eu_corp.trackColor();

        ///calculateAngle
        std::cout << "Angle me: " << ColorDetection::calculateAngle(eu_cap, eu_corp) << "\n";
        std::cout << "Angle me->enemy: " << ColorDetection::calculateAngle(eu_corp, inamic) << "\n";
    }

    return 0;
}


/*Ring detection - to be called once before the "battle" begins*/
void detectRing(Mat cameraFeed) {
    Mat src_gray;
    /// Convert it to gray
    cvtColor(cameraFeed, src_gray, CV_BGR2GRAY);

    /// Reduce the noise so we avoid false circle detection
    GaussianBlur(src_gray, src_gray, Size(9, 9), 2, 2);

    vector<Vec3f> circles;

    /// Apply the Hough Transform to find the circles
    HoughCircles(src_gray, circles, CV_HOUGH_GRADIENT, 1, src_gray.rows / 8, UPPER_THRES, CENTER_THRES, 0, 0);

    /// Find the biggest circle in the image, which must be the ring
    for (size_t i = 0; i < circles.size(); i++) {
        if (ring_radius < cvRound(circles[i][2])) {
            ring_radius = cvRound(circles[i][2]);
            ring_center = Point(cvRound(circles[i][0]), cvRound(circles[i][1]));
        }
    }

    /// Show ring for debug purposes
    // circle center
    circle(cameraFeed, ring_center, 3, Scalar(0, 255, 0), -1, 8, 0);
    // circle outline
    circle(cameraFeed, ring_center, ring_radius, Scalar(0, 0, 255), 3, 8, 0);

    namedWindow("Ring", CV_WINDOW_AUTOSIZE);
    imshow("Ring", cameraFeed);

    waitKey(30);
}

void createRingTrackbars(void) {
    std::string windowName = "Ring Detection Trackbars";
    namedWindow(windowName, 0);
    createTrackbar("UPPER_THRES", windowName, &UPPER_THRES, UPPER_THRES);
    createTrackbar("CENTER_THRES", windowName, &CENTER_THRES, CENTER_THRES);
}