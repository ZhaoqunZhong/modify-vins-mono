//
// Created by zhongzhaoqun on 2021/9/26.
//

#ifndef VIO_OB_SYSTEM_H
#define VIO_OB_SYSTEM_H

#include <string>
#include "opencv2/opencv.hpp"


class OB_SYSTEM {
public:
/**
* @brief    Initialize all resources needed by the algorithm, e.g creating threads, allocating memory.
* @param config_path     all-in-one config file for the algorithm
*/
    OB_SYSTEM(std::string config_path);

    /**
    * @brief Release resources, e.g. destroy threads, free memory.
    */
    ~OB_SYSTEM();

    /**
    * @brief Launch the algorithm, and ready to process data
    */
    void start();

    /**
    * @brief Shut down algorithm, output results, clear states, and ready to launch as new again.
    */
    void stop();

    /// Feed data to the algorithm, timestamp is in seconds.
    void rgbCallback(double ts, cv::Mat image);

    void imuCallback(double ts, double ax, double ay, double az, double wx, double wy, double wz);

    void accCallback(double ts, double ax, double ay, double az);

    void gyrCallback(double ts, double wx, double wy, double wz);

private:
    std::string config_path_;

};

#endif //VIO_OB_SYSTEM_H
