#include "ob_system.h"
#include "../system.h"
#include "native_debug.h"

std::shared_ptr<System> vins_;

OB_SYSTEM::OB_SYSTEM(std::string config_path) {
    vins_ = std::make_shared<System>(config_path);
}

OB_SYSTEM::~OB_SYSTEM() {
    vins_.reset();
}

void OB_SYSTEM::start() {
    vins_->vi_th_ = std::thread(&System::process, vins_);
    vins_->vi_th_.detach();
    vins_->mo_th_ = std::thread(&System::motionOnlyProcess, vins_);
    vins_->mo_th_.detach();
}

void OB_SYSTEM::stop() {
    vins_->bStart_backend = false;
    vins_->con.notify_one();
    vins_->mo_estimate_start = false;
    vins_->mo_buf_con_.notify_one();
    while (!vins_->process_exited || !vins_->mo_estimate_exited) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    vins_.reset();
}

void OB_SYSTEM::rgbCallback(double ts, cv::Mat image) {
    vins_->subImageData(ts, image);
}

void OB_SYSTEM::imuCallback(double ts, double ax, double ay, double az, double wx, double wy, double wz){
    Eigen::Vector3d acc{ax, ay, az};
    Eigen::Vector3d gyr{wx, wy, wz};
    vins_->subImuData(ts, gyr, acc);
}

void OB_SYSTEM::accCallback(double ts, double ax, double ay, double az){

}

void OB_SYSTEM::gyrCallback(double ts, double wx, double wy, double wz){

}