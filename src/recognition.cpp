#include <utility> // make_pair
#include <iostream>
#include <cassert>

#include "recognition.h"
#include "face_constants.h"

//#define DEBUG_recognition
#ifdef DEBUG_recognition
#include <iostream>
#include <opencv2/highgui/highgui.hpp>
#endif


using namespace cv;
using namespace std;

const double DESIRED_LEFT_EYE_X = 0.16;     // Controls how much of the face is visible after preprocessing.
const double DESIRED_LEFT_EYE_Y = 0.14;

static Mat norm_0_255(InputArray _src) {
    Mat src = _src.getMat();
    // Create and return normalized image:
    Mat dst;
    switch(src.channels()) {
    case 1:
        cv::normalize(_src, dst, 0, 255, NORM_MINMAX, CV_8UC1);
        break;
    case 3:
        cv::normalize(_src, dst, 0, 255, NORM_MINMAX, CV_8UC3);
        break;
    default:
        src.copyTo(dst);
        break;
    }
    return dst;
}

Recognizer::Recognizer(const FaceDetector& detector):
        _detector(detector)
{

    //int num_components = 0;
    //double threshold = 100.0;
    //model = createEigenFaceRecognizer(num_components, threshold);
    model = createEigenFaceRecognizer();
}

bool Recognizer::addPictureOf(const Mat& image, const string& label) {

    int idx = -1;

    for (const auto& kv : human_labels) {
        if (label == kv.second) {
            idx = kv.first;
            break;
        }
    }

    if (idx == -1) // new label!
    {
        idx = human_labels.size();
        trained_labels[idx] = false;
        human_labels[idx] = label;
    }


    if (trainingSet[idx].size() < MAX_TRAINING_IMAGES) {
        cout << "Acquiring " << trainingSet[idx].size() + 1 << "/" << MAX_TRAINING_IMAGES << " images for " << label << "... ";
        Mat preprocessedFace;

        if (preprocessFace(image, preprocessedFace)) {
            cout << "ok." << endl;
            trainingSet[idx].push_back(preprocessedFace);
        }
        else
            cout << "failed!" << endl;

        return false;
    }

    if (!trained_labels[idx])
    {
        cout << "Enough data for " << label << "! Training the recognizer..." << endl;
        train(idx);
    }
    return true;
}

void Recognizer::train(int label) {
    vector<Mat> images;
    vector<int> labels;

    for (const auto& image : trainingSet[label]) {
        labels.push_back(label);
        images.push_back(image);
    }

    model->train(images, labels);
    trained_labels[label] = true;
    cout << "I can now recognize " << human_labels[label] << " in new images." << endl;
}

pair<string, double> Recognizer::whois(const Mat& image) {

    if (trained_labels.empty()) return make_pair("", 0.0);

    int label = -1;
    double confidence = 0.0;

    Mat preprocessedFace;

    if (preprocessFace(image, preprocessedFace)) {
        model->predict(preprocessedFace, label, confidence);
    }
    else
    {
        //cerr << "Could not find the eyes!" << endl;
        return make_pair("", 0.0);
    }


    if (label >= 0)
        return make_pair(human_labels[label], confidence);
    else
        return make_pair("", 0.0);
}

/**
 * Code from Mastering OpenCV, Chapter 8
 */
bool Recognizer::preprocessFace(const Mat& faceImg, Mat& dstImg) {

    int desiredFaceHeight, desiredFaceWidth;

    // square faces
    desiredFaceHeight = desiredFaceWidth = FACE_WIDTH;

    // the image MUST be grayscale
    assert(faceImg.channels() == 1);
    
    // Search for the 2 eyes at the full resolution, since eye detection needs max resolution possible!
    Point leftEye, rightEye;
    bool eyes_detected = _detector.detectBothEyes(faceImg, leftEye, rightEye);
    
    // Check if both eyes were detected.
    if (!eyes_detected) {
        cout << "Eyes not detected!";
        return false;
    }

    // Make the face image the same size as the training images.

    // Since we found both eyes, lets rotate & scale & translate the face so that the 2 eyes
    // line up perfectly with ideal eye positions. This makes sure that eyes will be horizontal,
    // and not too far left or right of the face, etc.

    // Get the center between the 2 eyes.
    Point2f eyesCenter = Point2f( (leftEye.x + rightEye.x) * 0.5f, (leftEye.y + rightEye.y) * 0.5f );
    // Get the angle between the 2 eyes.
    double dy = (rightEye.y - leftEye.y);
    double dx = (rightEye.x - leftEye.x);
    double len = sqrt(dx*dx + dy*dy);
    double angle = atan2(dy, dx) * 180.0/CV_PI; // Convert from radians to degrees.

    // Hand measurements shown that the left eye center should ideally be at roughly (0.19, 0.14) of a scaled face image.
    const double DESIRED_RIGHT_EYE_X = (1.0f - DESIRED_LEFT_EYE_X);
    // Get the amount we need to scale the image to be the desired fixed size we want.
    double desiredLen = (DESIRED_RIGHT_EYE_X - DESIRED_LEFT_EYE_X) * desiredFaceWidth;
    double scale = desiredLen / len;
    // Get the transformation matrix for rotating and scaling the face to the desired angle & size.
    Mat rot_mat = getRotationMatrix2D(eyesCenter, angle, scale);
    // Shift the center of the eyes to be the desired center between the eyes.
    rot_mat.at<double>(0, 2) += desiredFaceWidth * 0.5f - eyesCenter.x;
    rot_mat.at<double>(1, 2) += desiredFaceHeight * DESIRED_LEFT_EYE_Y - eyesCenter.y;

    // Rotate and scale and translate the image to the desired angle & size & position!
    // Note that we use 'w' for the height instead of 'h', because the input face has 1:1 aspect ratio.
    Mat warped = Mat(desiredFaceHeight, desiredFaceWidth, CV_8U, Scalar(128)); // Clear the output image to a default grey.
    warpAffine(faceImg, warped, rot_mat, warped.size());
    //imshow("warped", warped);

    // Give the image a standard brightness and contrast, in case it was too dark or had low contrast.
    equalizeHist(warped, warped);
    //imshow("equalized", warped);

    // Use the "Bilateral Filter" to reduce pixel noise by smoothing the image, but keeping the sharp edges in the face.
    Mat filtered = Mat(warped.size(), CV_8U);
    bilateralFilter(warped, filtered, 0, 20.0, 2.0);
    //imshow("filtered", filtered);

    // Filter out the corners of the face, since we mainly just care about the middle parts.
    // Draw a filled ellipse in the middle of the face-sized image.
    Mat mask = Mat(warped.size(), CV_8U, Scalar(0)); // Start with an empty mask.
    Point faceCenter = Point( desiredFaceWidth/2, cvRound(desiredFaceHeight * FACE_ELLIPSE_CY) );
    Size size = Size( cvRound(desiredFaceWidth * FACE_ELLIPSE_W), cvRound(desiredFaceHeight * FACE_ELLIPSE_H) );
    ellipse(mask, faceCenter, size, 0, 0, 360, Scalar(255), -1); // tickness=-1 -> filled
    //imshow("mask", mask);

    // Use the mask, to remove outside pixels.
    dstImg = Mat(warped.size(), CV_8U, Scalar(128)); // Clear the output image to a default gray.

    // Apply the elliptical mask on the face.
    filtered.copyTo(dstImg, mask);  // Copies non-masked pixels from filtered to dstImg.

#ifdef DEBUG_recognition
    //namedWindow("filtered");
    //imshow("filtered", filtered);
    namedWindow("preprocessedface-debug");
    imshow("preprocessedface-debug", dstImg);
#endif


    return true;
}


// Generate an approximately reconstructed face by back-projecting the eigenvectors & eigenvalues of the given (preprocessed) face.
Mat Recognizer::reconstructFace(const Mat preprocessedFace)
{
    // Since we can only reconstruct the face for some types of FaceRecognizer models (ie: Eigenfaces or Fisherfaces),
    // we should surround the OpenCV calls by a try/catch block so we don't crash for other models.
    try {

        // Get some required data from the FaceRecognizer model.
        Mat eigenvectors = model->get<Mat>("eigenvectors");
        Mat averageFaceRow = model->get<Mat>("mean");

        int faceHeight = preprocessedFace.rows;

        // Project the input image onto the PCA subspace.
        Mat projection = subspaceProject(eigenvectors, averageFaceRow, preprocessedFace.reshape(1,1));
        //printMatInfo(projection, "projection");

        // Generate the reconstructed face back from the PCA subspace.
        Mat reconstructionRow = subspaceReconstruct(eigenvectors, averageFaceRow, projection);
        //printMatInfo(reconstructionRow, "reconstructionRow");

        // Convert the float row matrix to a regular 8-bit image. Note that we
        // shouldn't use "getImageFrom1DFloatMat()" because we don't want to normalize
        // the data since it is already at the perfect scale.

        // Make it a rectangular shaped image instead of a single row.
        Mat reconstructionMat = reconstructionRow.reshape(1, faceHeight);
        // Convert the floating-point pixels to regular 8-bit uchar pixels.
        Mat reconstructedFace = Mat(reconstructionMat.size(), CV_8U);
        reconstructionMat.convertTo(reconstructedFace, CV_8U, 1, 0);
        //printMatInfo(reconstructedFace, "reconstructedFace");

        return reconstructedFace;

    } catch (cv::Exception e) {
        //cout << "WARNING: Missing FaceRecognizer properties." << endl;
        return Mat();
    }
}

vector<Mat> Recognizer::eigenfaces() {

    vector<Mat> faces;

    Mat W = model->getMat("eigenvectors");
    // Display or save the Eigenfaces:
    for (int i = 0; i < min(10, W.cols); i++) {
        // get eigenvector #i
        Mat ev = W.col(i).clone();
        // Reshape to original size & normalize to [0...255] for imshow.
        Mat grayscale = norm_0_255(ev.reshape(1, FACE_WIDTH));
        // Show the image & apply a Jet colormap for better sensing.
        Mat cgrayscale;
        applyColorMap(grayscale, cgrayscale, COLORMAP_JET);
        faces.push_back(cgrayscale);
    }

    return faces;
}
