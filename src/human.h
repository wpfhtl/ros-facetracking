#ifndef HUMAN_H
#define HUMAN_H

#include <vector>
#include <string>
#include <opencv2/core/core.hpp>

#include "detection.h"
#include "recognition.h"

enum Mode {LOST, TRACKING};

class Human {

public:
    /** Initialize a new human, based on the bounding box of its face in the
     * current frame.
     */
    Human(const std::string& name, 
          const cv::Mat inputImage, 
          const cv::Rect boundingbox,
          Recognizer& faceRecognizer);

    /** Returns true if the given rectangle match my current bounding box.
     *
     * This is actually computed as a non-zero intersection area.
     */
    bool isMyself(const cv::Rect face) const;

    /** Returns the current analysis mode for this human: tracking, learning,...
     */
    Mode mode() const {return _mode;}

    /** Set the face bounding box, and initialize accordingly the offset between the
     * centroid of the tracked features and the boundingbox.
     */
    void relocalizeFace(const cv::Mat& image, const cv::Rect face);

    /** Update this face.
     *
     * This may mean:
     *  - track the face (ie, find the key points of the face in the current frame)
     */
    void update(const cv::Mat inputImage);

    void showFace(cv::Mat& ouputImage);

    std::string name() const {return _name;}

private:

    std::string _name;
    cv::Rect boundingbox;

    Mode _mode;

    FaceTracker tracker;
    //offset between the centroid of the tracked features and the actual face boundingbox.
    cv::Point trackerOffset;
    std::vector<cv::Point2f> features;
    Recognizer& faceRecognizer;
    bool recognizerTrained;
    
};

#endif // HUMAN_H
