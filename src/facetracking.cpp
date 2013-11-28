#include <opencv2/core/core.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <vector>
#include <map>

#include "detection.h"

//#define DEBUG_facetraking

using namespace cv;
using namespace std;

// The color (magenta) that will be used for all information
// overlaid on the captured image
const static cv::Scalar scColor(255, 0, 255);

// These constants will be given to OpenCv for drawing with
// sub-pixel accuracy with fixed point precision coordinates
static const int scShift = 16;
static const float scPrecision = 1<<scShift;

static const unsigned int FRAMES_BETWEEN_DETECTION = 50;

enum Mode {DETECT, TRACK};

int main(int argc, char *argv[])
{

    FaceDetector facedetector;

    cout << "Compiled with OpenCV version " << CV_VERSION << endl << endl;

    // Allow the user to specify a camera number, since not all computers will be the same camera number.
    int cameraIndex = 0;   // Change this if you want to use a different camera device.
    if (argc > 1) {
        cameraIndex = atoi(argv[1]);
    }

    // The source of input images
    cv::VideoCapture videoCapture(cameraIndex);
    if (!videoCapture.isOpened())
    {
        std::cerr << "Unable to initialise video capture." << std::endl;
        return 1;
    }

    videoCapture.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    videoCapture.set(CV_CAP_PROP_FRAME_HEIGHT, 480);


    namedWindow("faces"); 

    Mat cameraImage, inputImage;

    unsigned int frameCount = 0;

    map<int, FaceTracker> faceTrackers;

    Mode mode = DETECT;

    // Main loop, exiting when 'q is pressed'
    for (; 'q' != (char) cv::waitKey(1); ) {

        // Capture a new image.
        videoCapture.read(cameraImage);
        auto debugImage = cameraImage.clone();

        cvtColor(cameraImage, inputImage, CV_BGR2GRAY);

        int64 tStartCount = cv::getTickCount();

        if (mode == DETECT) // face detection!
        {
            faceTrackers.clear();

            auto faces = facedetector.detect(inputImage);


            for( auto i = 0 ; i < faces.size() ; i++ )
            {

                rectangle( debugImage, faces[i], scColor, 4 );

                auto features = facedetector.features(inputImage, faces[i]);
                faceTrackers.insert(pair<int, FaceTracker>(i, FaceTracker(inputImage, features)));

                for ( auto p : features ) {
                    line( debugImage, p, p, CV_RGB(10, 200, 100), 10 );
                }
            }

            if (faces.size() > 0) mode = TRACK;
        }
        else // face tracking!
        {
            for(auto& kv : faceTrackers) {
                auto features = kv.second.track(inputImage);

                auto rect = boundingRect(features);
                rectangle( debugImage, rect, CV_RGB(10, 100, 200), 4 );

                for (const auto& p : features ) {
                    line( debugImage, p, p, CV_RGB(10, 200, 100), 10 );
                }

#ifdef DEBUG_facetraking
                cout << "Tracking " << features.size() << " features" << endl;
#endif

                if (features.size() < 10) {
                    cout << "Not enough features! Going back to detection" << endl;
                    mode = DETECT;
                }
            }

        }

        std::cout << "Time to detect faces: " << ((double)cv::getTickCount() - tStartCount)/cv::getTickFrequency() << "ms" << std::endl;

        // Finally...
        cv::imshow("faces", debugImage);
        frameCount++;
    }

    cv::destroyWindow("faces");
    videoCapture.release();

}