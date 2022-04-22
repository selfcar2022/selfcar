//
// Created by sumin on 22. 4. 17.
//
#include "localizer/localizer.h"
using namespace std;


Localizer::Localizer()
    :nh_(""), pnh_("")
{
    // 미리 선언
    std::string gps_subscribe_topic_name;
    std::string gps_bestvel_subscribe_topic_name;
    std::string gps_bestpos_subscribe_topic_name;
    std::string initialpose_topic_name;
    std::string Velocity_topic_name;
    std::string imu_subscribe_topic_name;

    bool gps_velocity_estimate;
    // Gps - Param
    pnh_.param<std::string>("gps_subscribe_topic_name",gps_subscribe_topic_name,"/novatel_fix");
    pnh_.param<std::string>("gps_bestvel_subscribe_topic_name",gps_bestvel_subscribe_topic_name,"/bestvel");
    pnh_.param<std::string>("gps_bestpos_subscribe_topic_name",gps_bestpos_subscribe_topic_name,"/bestpos");
    pnh_.param<std::string>("Velocity_topic_name",Velocity_topic_name,"/MSG_CON/Rx_Vel");
    pnh_.param<int>("covariance_sample_num", sampleNum, 3);
    pnh_.param<bool>("gps_velocity_estimate",gps_velocity_estimate,true);

    //IMU - Param
    pnh_.param<std::string>("imu_subscribe_topic_name", imu_subscribe_topic_name, "/ebimu_");
    //Encoder -Param
    pnh_.param<std::string>("initialpose_topic_name",initialpose_topic_name,"/initialpose");
    // Map - Param
    pnh_.param<double>("utmOffset_x",utmoffsetX,361001.412425);
    pnh_.param<double>("utmOffset_y",utmoffsetY,4065821.07176);

    // ROS - SUB - GPS
    subGPS = nh_.subscribe(gps_subscribe_topic_name, 1, &Localizer::gpsCallback, this);
    subBestPos = nh_.subscribe(gps_bestpos_subscribe_topic_name,1, &Localizer::bestposCallback, this);
    subBestVel = nh_.subscribe(gps_bestvel_subscribe_topic_name,1, &Localizer::bestvelCallback, this );
    subInitialpose = nh_.subscribe(initialpose_topic_name,1,&Localizer::initialposeCallback,this);
    subVel = nh_.subscribe(Velocity_topic_name,1,&Localizer::velCallback, this);

    // ROS - SUB - IMU
    subIMU = nh_.subscribe(imu_subscribe_topic_name,1, &Localizer::imuCallback, this);



    pubPose = nh_.advertise<geometry_msgs::PoseWithCovariance>("Perception/Localization/LocalPose",1);
    pubMarker = nh_.advertise<visualization_msgs::Marker>("Perception/Localization/Marker", 1);
    pubAxis = nh_.advertise<nav_msgs::Odometry>("Perception/Localization/LocalAxis",1);
    pubImupos= nh_.advertise<nav_msgs::Odometry>("Perception/Localization/ImuAxis",1);
    pubMarker_filtered = nh_.advertise<visualization_msgs::Marker>("Perception/Localization/Marker_filtered", 1);

    // GPS - DATA
    gpsSample =Eigen::MatrixXd::Zero(sampleNum, dim_gps);
    gpsData = Eigen::MatrixXd::Zero(dim_gps,1);
    gpsCov = Eigen::MatrixXd::Zero(dim_gps, dim_gps);

    // gps - flag
    bInitgps = true;
    bInitgpsSample = false;
    bInitvelSample = false;
    isGPSstop = true;
    isGPSavailable = false;

    //visualizer
    markerId = 0;
    markerId_filtered =0;


    std::cout.precision(15);

}
void Localizer::imuCallback(const sensor_msgs::Imu::ConstPtr &msg){
    imu = *msg;
    double r,p,y;
    tf2::Quaternion q ;
    tf::Quaternion rpy;
    tf::quaternionMsgToTF(msg->orientation, rpy);
    tf::Matrix3x3(rpy).getRPY(r,p,y);

    //q.setRPY(r,p,y);
    //q.normalize();
    q.setRPY(0,0,y);


    nav_msgs::Odometry Axis;
    Axis.header.frame_id = "map";
    Axis.header.stamp = ros::Time::now();
    Axis.pose.pose.position.x = gpsData(GPS::X)-utmoffsetX;
    Axis.pose.pose.position.y = gpsData(GPS::Y)-utmoffsetY;
    Axis.pose.pose.position.z = 0;

    Axis.pose.pose.orientation = tf2::toMsg(q);
    pubImupos.publish(Axis);
}

void Localizer::velCallback(const std_msgs::Int16::ConstPtr & msg){
    vel = *msg;

}
void Localizer::gpsCallback(const sensor_msgs::NavSatFix::ConstPtr &msg) {
    gps = *msg;
    double utm_x_meas = 0.;
    double utm_y_meas = 0.;

    std::string utm_zone;
    RobotLocalization::NavsatConversions::LLtoUTM(gps.latitude,gps.longitude, utm_y_meas, utm_x_meas, utm_zone);
    if(bInitgps)
    {
        gpsData(GPSIDX::X) = utm_x_meas;
        gpsData(GPSIDX::Y) = utm_y_meas;
        gpsData(GPSIDX::YAW) = 0;
        gpsData(GPSIDX::V) = 0;
        bInitgpsSample = true;
        bInitvelSample = true;
        bInitgps = false;
        timeGPSprev = ros::Time::now();
        return;
    }
    double theta = std::atan2(utm_y_meas - gpsData(GPSIDX::Y), utm_x_meas - gpsData(GPSIDX::X));

    theta  = std::atan2(sin(theta), cos(theta));
    gpsData(GPSIDX::X) = utm_x_meas;
    gpsData(GPSIDX::Y) = utm_y_meas;
    if (vel.data > 40) {
        if (filter_yaw.size()>3){
            filter_yaw.erase(filter_yaw.begin());
        }
        filter_yaw.push_back(theta);
        double sum = 0;
        double sum2 =0;
        for (int i=0;i<3;i++){
            sum += filter_yaw[i]*pow(0.1,pow(2,3-i));
            sum2 += pow(0.1,pow(2,3-i));}
        theta = double(double(sum)/double(sum2));
        gpsData(GPSIDX::YAW) = theta;
    }
    if(bInitgpsSample)
    {
        gpsSample.block(0,GPSIDX::X,sampleNum,1) = Eigen::MatrixXd::Constant(sampleNum,1,gpsData(GPSIDX::X));
        gpsSample.block(0,GPSIDX::Y,sampleNum,1) = Eigen::MatrixXd::Constant(sampleNum,1,gpsData(GPSIDX::Y));
        gpsSample.block(0,GPSIDX::YAW,sampleNum,1) = Eigen::MatrixXd::Constant(sampleNum,1,gpsData(GPSIDX::YAW));
        bInitgpsSample = false;
    }

    Eigen::MatrixXd data = gpsData.transpose();
    getCovariance(gpsSample, data, gpsCov);

    tf2::Quaternion q_gps;
    q_gps.setRPY(0,0,gpsData(GPS::YAW));

    visualization_msgs::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = ros::Time::now();
    marker.ns = "raw";
    marker.id = markerId++;
    marker.type = visualization_msgs::Marker::ARROW;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose.position.x = gpsData(GPS::X)- utmoffsetX;
    marker.pose.position.y = gpsData(GPS::Y)-utmoffsetY;
    marker.pose.position.z = 0;

    marker.pose.orientation = tf2::toMsg(q_gps);
    marker.scale.x = 0.2;
    marker.scale.y = 0.1;
    marker.scale.z = 0.1;

    //if(markerId > 200) markerId = 0;

    // Set the color -- be sure to set alpha to something non-zero!
    marker.color.r = 0.0f;
    marker.color.g = 1.0f;
    marker.color.b = 0.0f;
    marker.color.a = 1.0;
    pubMarker.publish(marker);

    static tf2_ros::TransformBroadcaster br;
    geometry_msgs::TransformStamped tStamp;
    tStamp.header.stamp = ros::Time::now();
    tStamp.header.frame_id = "map";
    tStamp.child_frame_id = "gps_raw";
    tStamp.transform.translation.x = gpsData(GPS::X)-utmoffsetX;
    tStamp.transform.translation.y = gpsData(GPS::Y)-utmoffsetY;
    tStamp.transform.translation.z = 0;
    tStamp.transform.rotation = tf2::toMsg(q_gps);
    br.sendTransform(tStamp);




    gps_pos.pose.position.x = gpsData(GPS::X);
    gps_pos.pose.position.y = gpsData(GPS::Y);
    gps_pos.pose.position.z = 0;

    tf2::Quaternion q ;
    q.setRPY(0,0,gpsData(GPS::YAW));
    cout << "YAW "<< gpsData(GPS::YAW) <<endl;
    q.normalize();
    gps_pos.pose.orientation = tf2::toMsg(q);
    pubPose.publish(gps_pos);

    nav_msgs::Odometry Axis;
    Axis.header.frame_id = "map";
    Axis.header.stamp = ros::Time::now();
    Axis.pose.pose.position.x = gpsData(GPS::X)-utmoffsetX;
    Axis.pose.pose.position.y = gpsData(GPS::Y)-utmoffsetY;
    Axis.pose.pose.position.z = 0;

    Axis.pose.pose.orientation = tf2::toMsg(q);
    pubAxis.publish(Axis);



}
void Localizer::bestposCallback(const novatel_gps_msgs::NovatelPosition::ConstPtr& msg) {
    bestpos = *msg;

}
void Localizer::bestvelCallback(const novatel_gps_msgs::NovatelVelocity::ConstPtr& msg) {
    bestvel = *msg;
    gpsData(GPSIDX::V) = bestvel.horizontal_speed;
}
void Localizer::initialposeCallback(const geometry_msgs::PoseWithCovarianceStamped::ConstPtr &msg) {
    double roll, pitch, yaw;

    tf2::Quaternion q(
            msg->pose.pose.orientation.x,
            msg->pose.pose.orientation.y,
            msg->pose.pose.orientation.z,
            msg->pose.pose.orientation.w
    );
    tf2::Matrix3x3 m(q);
    m.getRPY(roll, pitch, yaw);
    std::cout << "RPY : " << roll << ' ' << pitch << ' ' << yaw << '\n';

}
void Localizer::visualizerCallback() {

    gps_pos.pose.position.x = gpsData(GPS::X);
    gps_pos.pose.position.y = gpsData(GPS::Y);
    gps_pos.pose.position.z = 0;
    tf2::Quaternion q ;
    q.setRPY(0,0,gpsData(GPS::YAW));
    q.normalize();
    gps_pos.pose.orientation = tf2::toMsg(q);

    pubPose.publish(gps_pos);
    nav_msgs::Odometry Axis;
    Axis.header.frame_id = "map";
    Axis.header.stamp = ros::Time::now();
    Axis.pose.pose.position.x = gpsData(GPS::X)-utmoffsetX;
    Axis.pose.pose.position.y = gpsData(GPS::Y)-utmoffsetY;
    Axis.pose.pose.position.z = 0;

    Axis.pose.pose.orientation = tf2::toMsg(q);
    pubAxis.publish(Axis);

    tf2::Quaternion q_gps;
    q_gps.setRPY(0,0,gpsData(GPS::YAW));

    visualization_msgs::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = ros::Time::now();
    marker.ns = "baisic_shapes";
    marker.id = markerId++;
    marker.type = visualization_msgs::Marker::ARROW;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose.position.x = gpsData(GPS::X)- utmoffsetX;
    marker.pose.position.y = gpsData(GPS::Y)-utmoffsetY;
    marker.pose.position.z = 0;
    marker.pose.orientation = tf2::toMsg(q_gps);
    marker.scale.x = 0.2;
    marker.scale.y = 0.1;
    marker.scale.z = 0.1;

    // Set the color -- be sure to set alpha to something non-zero!
    marker.color.r = 0.0f;
    marker.color.g = 1.0f;
    marker.color.b = 0.0f;
    marker.color.a = 1.0;
    pubMarker.publish(marker);



}