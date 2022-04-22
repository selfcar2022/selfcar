//
// Created by sumin on 22. 4. 17.
//

#ifndef SRC_LOCALIZER_H
#define SRC_LOCALIZER_H
#include "covariance.h"
#include "navsat_conversions.h"
#include <sensor_msgs/NavSatFix.h>
#include <sensor_msgs/Imu.h>
#include <geometry_msgs/PoseWithCovariance.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <std_msgs/Int16.h>
#include <nav_msgs/Odometry.h>
#include <std_msgs/Int16.h>
#include <std_msgs/Int32.h>
#include <novatel_gps_msgs/NovatelVelocity.h>
#include <novatel_gps_msgs/NovatelPosition.h>

#include <tf/transform_broadcaster.h>
#include <tf2/utils.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2/convert.h>
#include <visualization_msgs/Marker.h>


#include <vector>
#include <iomanip>

class Localizer{
private:
    //GPS
    void gpsCallback(const sensor_msgs::NavSatFix::ConstPtr& msg);
    void bestvelCallback(const novatel_gps_msgs::NovatelVelocity::ConstPtr& msg);
    void bestposCallback(const novatel_gps_msgs::NovatelPosition::ConstPtr& msg);
    void initialposeCallback(const geometry_msgs::PoseWithCovarianceStamped::ConstPtr& msg);

    //IMU
    void imuCallback(const sensor_msgs::Imu::ConstPtr& msg);

    // Encoder
    void velCallback(const std_msgs::Int16::ConstPtr & msg);

    // ros
    ros::NodeHandle nh_;
    ros::NodeHandle pnh_;

    // ROS- GPS
    ros::Subscriber subGPS;
    ros::Subscriber subBestVel;
    ros::Subscriber subBestPos;
    ros::Subscriber subInitialpose;

    // ROS - IMU
    ros::Subscriber subIMU;

    // IMU - DATA

    sensor_msgs::Imu imu;
    ros::Time timeIMUprev;
    //Encoder
    ros::Subscriber subVel;

    // ROS- Visualization
    ros::Publisher pubPose;
    ros::Publisher pubMarker;
    ros::Publisher pubAxis;
    ros::Publisher pubImupos;
    ros::Publisher pubMarker_filtered;

    // gps - data
    int dim_gps = 4;
    struct GPS
    {
        enum IDX
        {
            X = 0,
            Y = 1,
            YAW = 2,
            V = 3
        };
    };
    typedef GPS::IDX GPSIDX;
    sensor_msgs::NavSatFix gps;
    novatel_gps_msgs::NovatelVelocity bestvel;
    novatel_gps_msgs::NovatelPosition bestpos;
    Eigen::MatrixXd gpsSample;
    Eigen::MatrixXd gpsData; // x y theta velocity
    Eigen::MatrixXd gpsCov; // covariance;
    ros::Time timeGPSprev;
    double timeGPSelapsed;
    int sampleNum;
    std::vector<double> filter_yaw;
    // gps - flag
    bool bInitgps;
    bool bInitgpsSample;
    bool bInitvelSample;
    bool isGPSstop;
    bool isGPSavailable;

    // gps - param
    double utmoffsetX; // meter
    double utmoffsetY; // meter

    //Encoder
    std_msgs::Int16 vel;


    // visualizer
    void visualizerCallback();
    int markerId;
    int markerId_filtered;

    //pos
    geometry_msgs::PoseWithCovariance gps_pos;
    

public:
    Localizer();

};


#endif //SRC_LOCALIZER_H
