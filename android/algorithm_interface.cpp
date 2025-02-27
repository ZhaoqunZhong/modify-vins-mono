#include <unistd.h>
#include <memory>
#include <thread>
#include <chrono>
#include "glog/logging.h"
#include <dirent.h>
#include "MessageType/sensor_msgs/Image.h"
#include "MessageType/sensor_msgs/CompressedImage.h"
#include "MessageType/sensor_msgs/Imu.h"
#include "RosbagStorage/rosbag/view.h"
#include "cv_bridge_simple.h"
#include "../../algorithm_interface.h"
#include "native_debug.h"
#include "ob_system.h"

// #define IS_SEPARATE_IMU
// #define GLOG_TO_FILE

std::shared_ptr <OB_SYSTEM> slam_;

ob_slam::rosbag::Bag play_bag;
#define DEFAULT_TOPIC_IMU   "/imu0"
#define DEFAULT_TOPIC_IMAGE "/cam0/image_raw"
#define DEFAULT_TOPIC_ACCEL "/acc0"
#define DEFAULT_TOPIC_GYRO  "/gyr0"
std::vector <std::string> ros_topics{DEFAULT_TOPIC_IMU, DEFAULT_TOPIC_IMAGE, DEFAULT_TOPIC_ACCEL, DEFAULT_TOPIC_GYRO};


void AlgorithmInterface::rgbCallback(rgb_msg &msg) {
    if (!algorithm_on_)
        return;
    if (slam_ == nullptr)
        return;

    slam_->rgbCallback(msg.ts / 1e9, msg.yMat);
}

void AlgorithmInterface::featureCallback(double ts, std::vector <std::pair<size_t, Eigen::VectorXf>> &feats) {
    if (!algorithm_on_)
        return;
    if (slam_ == nullptr)
        return;
}

void AlgorithmInterface::imuCallback(imu_msg &msg) {
    if (!algorithm_on_)
        return;
    if (slam_ == nullptr)
        return;
#ifndef IS_SEPARATE_IMU
    slam_->imuCallback(msg.ts / 1e9, msg.acc_part.ax, msg.acc_part.ay, msg.acc_part.az,
                           msg.gyro_part.rx, msg.gyro_part.ry, msg.gyro_part.rz);
#endif
}

void AlgorithmInterface::accCallback(acc_msg &msg) {
    if (!algorithm_on_)
        return;
    if (slam_ == nullptr)
        return;
#ifdef IS_SEPARATE_IMU
    slam_->accCallback(msg.ts/1e9, msg.ax, msg.ay, msg.az);
#endif
}

void AlgorithmInterface::gyrCallback(gyr_msg &msg) {
    if (!algorithm_on_)
        return;
    if (slam_ == nullptr)
        return;
#ifdef IS_SEPARATE_IMU
    slam_->gyrCallback(msg.ts/1e9, msg.rx, msg.ry, msg.rz);
#endif
}

void *threadRunner(void *ptr) {
    AlgorithmInterface *classptr = (AlgorithmInterface *) ptr;
    classptr->runAlgorithm();
    return nullptr;
}

void AlgorithmInterface::start() {
//    start_stdcout_logger();
#ifdef GLOG_TO_FILE
    std::string log_file = "/sdcard/orbbec-vio-data/log/log";
    google::SetLogDestination(google::GLOG_INFO, "");
    google::SetLogDestination(google::GLOG_ERROR, "");
    google::SetLogDestination(google::GLOG_FATAL, "");
    google::SetLogDestination(google::GLOG_WARNING, log_file.c_str());
    google::InitGoogleLogging("");
#endif
    std::string file_name = "sdcard/orbbec-vio-data/config.yaml";
    cv::FileStorage fs;
    try {
        fs.open(file_name, cv::FileStorage::READ);
    } catch (const cv::Exception &e) {
        LOG(ERROR) << e.what();
    }
    run_offline_ = static_cast<int>(fs["run_offline"]);
    std::string config;
    if (run_offline_)
        config = static_cast<std::string>(fs["offline_config"]);
    else
        config = static_cast<std::string>(fs["online_config"]);
    bag_name_ = static_cast<std::string>(fs["rosbag_path"]);
    fs.release();

    slam_ = std::make_shared<OB_SYSTEM>(config);
    slam_->start();

    algorithm_on_ = true;
    pthread_create(&algo_t_, nullptr, threadRunner, this);
}

void AlgorithmInterface::stop() {
    algorithm_on_ = false;
    pthread_join(algo_t_, nullptr);

    slam_->stop();
    slam_.reset();

#ifdef GLOG_TO_FILE
    google::FlushLogFiles(google::GLOG_WARNING);
    google::ShutdownGoogleLogging();
#endif

    // clearVisualizationBuffers();
}


void AlgorithmInterface::runAlgorithm() {
    if (run_offline_) {
        LOG(INFO) << "opening rosbag: " << bag_name_ << std::endl;
        play_bag.open(bag_name_, static_cast<uint32_t>(ob_slam::rosbag::BagMode::Read));
        ob_slam::rosbag::View view(play_bag, ob_slam::rosbag::TopicQuery(ros_topics),
                                   ob_slam::TIME_MIN, ob_slam::TIME_MAX);
        ob_slam::rosbag::View::iterator iter, next_iter;
        for (iter = view.begin(), next_iter = view.begin(); iter != view.end(); iter++) {
            // LOG_FIRST_N(INFO, 100) << "topic " << iter->getTopic();
            if (DEFAULT_TOPIC_IMAGE == iter->getTopic()) {
                if (iter->isType<ob_slam::sensor_msgs::Image>()) {
                    LOG_FIRST_N(WARNING, 1) << "sensor_msgs::Image detected";
                    ob_slam::sensor_msgs::Image::ConstPtr image_ptr = iter->instantiate<ob_slam::sensor_msgs::Image>();
                    CvBridgeSimple cvb;
                    cv::Mat mat = cvb.ConvertToCvMat(image_ptr);
                    double ts = image_ptr->header.stamp.toSec();
                    if (slam_ == nullptr)
                        return;
                    slam_->rgbCallback(ts, mat);
                } else if (iter->isType<ob_slam::sensor_msgs::CompressedImage>()) { ///compressed image
                    LOG_FIRST_N(WARNING, 1) << "sensor_msgs::CompressedImage detected";
                    ob_slam::sensor_msgs::CompressedImage::ConstPtr image_ptr = iter->instantiate<ob_slam::sensor_msgs::CompressedImage>();
                    cv::Mat mat = cv::imdecode(image_ptr->data, cv::IMREAD_GRAYSCALE);
                    double ts = image_ptr->header.stamp.toSec();
                    if (slam_ == nullptr)
                        return;
                    slam_->rgbCallback(ts, mat);
                } else {
                    LOG(ERROR) << "Unknow image type in ros bag!";
                    return;
                }
            }
#ifndef IS_SEPARATE_IMU
            if (DEFAULT_TOPIC_IMU == iter->getTopic()) {
                LOG_FIRST_N(WARNING, 1) << "sensor_msgs::Imu detected";
                ob_slam::sensor_msgs::Imu::ConstPtr imu_ptr = iter->instantiate<ob_slam::sensor_msgs::Imu>();
                if (slam_ == nullptr)
                    return;
                slam_->imuCallback(imu_ptr->header.stamp.toSec(),
                                       imu_ptr->linear_acceleration.x,
                                       imu_ptr->linear_acceleration.y,
                                       imu_ptr->linear_acceleration.z, imu_ptr->angular_velocity.x,
                                       imu_ptr->angular_velocity.y, imu_ptr->angular_velocity.z);
            }
#else
            if (DEFAULT_TOPIC_ACCEL == iter->getTopic()) {
                ob_slam::geometry_msgs::Vector3Stamped::ConstPtr
                        acc_ptr = iter->instantiate<ob_slam::geometry_msgs::Vector3Stamped>();
                if (slam_ == nullptr)
                    return;
                slam_->accCallback(acc_ptr->header.stamp.toSec(), acc_ptr->vector.x, acc_ptr->vector.y, acc_ptr->vector.z);
            }
            if (DEFAULT_TOPIC_GYRO == iter->getTopic()) {
                ob_slam::geometry_msgs::Vector3Stamped::ConstPtr
                        gyr_ptr = iter->instantiate<ob_slam::geometry_msgs::Vector3Stamped>();
                if (slam_ == nullptr)
                        return;
                slam_->gyrCallback(gyr_ptr->header.stamp.toSec(), gyr_ptr->vector.x, gyr_ptr->vector.y, gyr_ptr->vector.z);
            }
#endif
            /// sleep between messages
            if (++next_iter != view.end()) {
                double sleep_time = next_iter->getTime().toSec() - iter->getTime().toSec();
                int play_rate = 2;
                sleep_time = sleep_time / play_rate;
                struct timespec ts;
                ts.tv_sec = (long) sleep_time;
                ts.tv_nsec = (long) ((sleep_time - (long) sleep_time) * 1e9);
                nanosleep(&ts, 0);
            }

            if (!algorithm_on_)
                break;
        }

        play_bag.close();
        LOG(INFO) << "Ros bag closed: " << bag_name_;
    }

}

