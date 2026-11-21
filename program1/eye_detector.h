// Detect face landmarks, eye closure, mouth opening and approximate gaze from camera frames.
#pragma once

#include <opencv2/opencv.hpp>
#include <array>
#include <cmath>
#include <filesystem>

struct EyeResult {
    bool valid = false;
    bool gazeValid = false;
    bool lookingAway = false;
    double left = -1, right = -1, pitch = 0, yaw = 0;
    double blinkLeft=0, blinkRight=0, jawOpen=0;
    std::vector<cv::Point2f> overlay;
    std::string status = "Face/eyes unavailable";
};

inline double pointDistance2D(const std::vector<cv::Vec3f>& points, int first, int second) {
    return std::hypot(static_cast<double>(points[first][0]-points[second][0]),
                      static_cast<double>(points[first][1]-points[second][1]));
}

inline double eyeEAR2D(const std::vector<cv::Vec3f>& p, const std::array<int,6>& e) {

    const double width=pointDistance2D(p,e[0],e[3]);
    if(!std::isfinite(width)||width<4)return -1;
    return (pointDistance2D(p,e[1],e[5])+pointDistance2D(p,e[2],e[4]))/(2*width);
}
inline cv::Point2d eyeGazeRatio(const std::vector<cv::Vec3f>& points, int iris,
                                    const std::array<int,6>& eye) {
    cv::Point2d first(points[eye[0]][0], points[eye[0]][1]);
    cv::Point2d last(points[eye[3]][0], points[eye[3]][1]);
    cv::Point2d top((points[eye[1]][0] + points[eye[2]][0]) * 0.5,
                    (points[eye[1]][1] + points[eye[2]][1]) * 0.5);
    cv::Point2d bottom((points[eye[4]][0] + points[eye[5]][0]) * 0.5,
                       (points[eye[4]][1] + points[eye[5]][1]) * 0.5);
    cv::Point2d center(points[iris][0], points[iris][1]);
    cv::Point2d horizontal = last - first;
    cv::Point2d vertical = bottom - top;
    if (horizontal.dot(horizontal) < 16 || vertical.dot(vertical) < 4) return {-1,-1};
    return {(center-first).dot(horizontal) / horizontal.dot(horizontal),
            (center-top).dot(vertical) / vertical.dot(vertical)};
}

class EyeDetector {
    cv::Ptr<cv::FaceDetectorYN> detector;
    cv::dnn::Net mesh, blend;
public:
    explicit EyeDetector(const std::filesystem::path& models) {
        detector = cv::FaceDetectorYN::create((models / "yunet.onnx").string(), "", {320, 240}, 0.8f);
        mesh = cv::dnn::readNetFromONNX((models / "face_mesh.onnx").string());
        mesh.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        mesh.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        blend=cv::dnn::readNetFromONNX((models/"face_blendshapes.onnx").string());
        blend.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        blend.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }

    EyeResult detect(const cv::Mat& frame) {
        EyeResult result;
        cv::Mat small, faces;
        cv::resize(frame, small, {320, cvRound(frame.rows * 320.0 / frame.cols)});
        detector->setInputSize(small.size());
        detector->detect(small, faces);
        if (faces.rows != 1) {
            result.status = faces.rows > 1 ? "Multiple faces - paused" : "No face - paused";
            return result;
        }
        const float* f = faces.ptr<float>();
        const double scale = frame.cols / 320.0;
        cv::Point2f eyeA(f[4] * scale, f[5] * scale), eyeB(f[6] * scale, f[7] * scale);
        if (eyeA.x > eyeB.x) std::swap(eyeA, eyeB);
        const double angle = std::atan2(eyeB.y - eyeA.y, eyeB.x - eyeA.x) * 180 / CV_PI;
        const double side = 1.5 * std::max(f[2], f[3]) * scale;
        if (side < 80) { result.status = "Face too small - move closer"; return result; }
        cv::Point2f center((f[0] + f[2] / 2) * scale, (f[1] + f[3] / 2) * scale);
        cv::Mat warp = cv::getRotationMatrix2D(center, angle, 256 / side);
        warp.at<double>(0, 2) += 128 - center.x;
        warp.at<double>(1, 2) += 128 - center.y;
        cv::Mat crop, inverse;
        cv::warpAffine(frame, crop, warp, {256, 256});
        cv::invertAffineTransform(warp, inverse);
        mesh.setInput(cv::dnn::blobFromImage(crop, 1.0 / 255, {256, 256}, {}, true));
        std::vector<cv::Mat> outputs;
        mesh.forward(outputs, mesh.getUnconnectedOutLayersNames());
        cv::Mat coordinates;
        float logit = -100;
        for (const auto& out : outputs) {
            if (out.total() == 478 * 3) coordinates = out;
            else if (out.total() == 1) logit = out.ptr<float>()[0];
        }

        if (coordinates.empty() || !std::isfinite(logit) || logit < 0) {
            result.status = "Mesh unreliable - paused"; return result;
        }
        const float* raw = coordinates.ptr<float>();
        std::vector<cv::Vec3f> points(478);
        for (int i = 0; i < 478; ++i) {
            points[i] = {raw[3 * i], raw[3 * i + 1], raw[3 * i + 2]};
            for (int k = 0; k < 3; ++k)
                if (!std::isfinite(points[i][k])) return result;
        }
        const std::array<int, 6> left = {33, 160, 158, 133, 153, 144};
        const std::array<int, 6> right = {362, 385, 387, 263, 373, 380};
        result.left = eyeEAR2D(points, left);
        result.right = eyeEAR2D(points, right);
        cv::Vec3f normal = (points[263] - points[33]).cross(points[152] - points[10]);
        if (cv::norm(normal) < 1e-5) return result;
        result.pitch = std::atan2(normal[1], std::abs(normal[2])) * 180 / CV_PI;
        result.yaw = std::atan2(normal[0], std::abs(normal[2])) * 180 / CV_PI;

        for (const auto& eye : {left, right}) for (int i : eye) {
            const double x = points[i][0], y = points[i][1];
            cv::Point2f pixel(inverse.at<double>(0,0)*x + inverse.at<double>(0,1)*y + inverse.at<double>(0,2),
                              inverse.at<double>(1,0)*x + inverse.at<double>(1,1)*y + inverse.at<double>(1,2));
            if (pixel.x < 0 || pixel.y < 0 || pixel.x >= frame.cols || pixel.y >= frame.rows) return result;
            result.overlay.push_back(pixel);
        }

        static const std::array<int,146> subset = {

    0,   1,   4,   5,   6,   7,   8,   10,  13,  14,  17,  21,  33,  37,  39,
    40,  46,  52,  53,  54,  55,  58,  61,  63,  65,  66,  67,  70,  78,  80,
    81,  82,  84,  87,  88,  91,  93,  95,  103, 105, 107, 109, 127, 132, 133,
    136, 144, 145, 146, 148, 149, 150, 152, 153, 154, 155, 157, 158, 159, 160,
    161, 162, 163, 168, 172, 173, 176, 178, 181, 185, 191, 195, 197, 234, 246,
    249, 251, 263, 267, 269, 270, 276, 282, 283, 284, 285, 288, 291, 293, 295,
    296, 297, 300, 308, 310, 311, 312, 314, 317, 318, 321, 323, 324, 332, 334,
    336, 338, 356, 361, 362, 365, 373, 374, 375, 377, 378, 379, 380, 381, 382,
    384, 385, 386, 387, 388, 389, 390, 397, 398, 400, 402, 405, 409, 415, 454,
    466, 468, 469, 470, 471, 472, 473, 474, 475, 476, 477
        };
        int inputShape[]={1,146,2};
        cv::Mat input(3,inputShape,CV_32F);
        for(int i=0;i<146;++i){
            const double x=points[subset[i]][0],y=points[subset[i]][1];
            input.ptr<float>()[2*i]=static_cast<float>(
                inverse.at<double>(0,0)*x+inverse.at<double>(0,1)*y+inverse.at<double>(0,2));
            input.ptr<float>()[2*i+1]=static_cast<float>(
                inverse.at<double>(1,0)*x+inverse.at<double>(1,1)*y+inverse.at<double>(1,2));
        }
        blend.setInput(input);
        cv::Mat expression=blend.forward();
        if(expression.total()!=52){result.status="Expression output invalid";return result;}
        const float* coefficients=expression.ptr<float>();
        for(int i=0;i<52;++i)
            if(!std::isfinite(coefficients[i])){result.status="Expression unreliable";return result;}
        result.blinkLeft=std::clamp(static_cast<double>(coefficients[9]),0.0,1.0);
        result.blinkRight=std::clamp(static_cast<double>(coefficients[10]),0.0,1.0);
        result.jawOpen=std::clamp(static_cast<double>(coefficients[25]),0.0,1.0);

        result.valid = result.left >= 0 && result.right >= 0 && result.left < 0.7 && result.right < 0.7;
        if (result.valid && std::max(result.blinkLeft, result.blinkRight) <= 0.35 &&
            result.left > 0.12 && result.right > 0.12) {
            cv::Point2d leftGaze = eyeGazeRatio(points, 468, left);
            cv::Point2d rightGaze = eyeGazeRatio(points, 473, right);
            result.gazeValid = leftGaze.x >= 0 && rightGaze.x >= 0 &&
                               std::isfinite(leftGaze.y) && std::isfinite(rightGaze.y);
            if (result.gazeValid) {
                result.lookingAway = (leftGaze.x < 0.25 && rightGaze.x < 0.25) ||
                                     (leftGaze.x > 0.75 && rightGaze.x > 0.75) ||
                                     (leftGaze.y < 0.05 && rightGaze.y < 0.05) ||
                                     (leftGaze.y > 0.95 && rightGaze.y > 0.95);
            }
        }
        result.status = result.valid ? "Tracking eyes + expression" : "Eye geometry unreliable";
        return result;
    }
};
