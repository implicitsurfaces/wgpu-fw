#include <iostream>

#include <stereo/app/solver.h>

using namespace stereo;

int main(int argc, char** argv) {
    CaptureRef cap = std::make_shared<cv::VideoCapture>(0);
    StereoSolver solver {{cap}};
    solver.capture(0);
    std::cout << "finished!" << std::endl;
}
