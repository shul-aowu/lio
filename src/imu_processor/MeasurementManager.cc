/**
* This file is part of LIO-mapping.
* 
* Copyright (C) 2019 Haoyang Ye <hy.ye at connect dot ust dot hk>,
* Robotics and Multiperception Lab (RAM-LAB <https://ram-lab.com>),
* The Hong Kong University of Science and Technology
* 
* For more information please see <https://ram-lab.com/file/hyye/lio-mapping>
* or <https://sites.google.com/view/lio-mapping>.
* If you use this code, please cite the respective publications as
* listed on the above websites.
* 
* LIO-mapping is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* LIO-mapping is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with LIO-mapping.  If not, see <http://www.gnu.org/licenses/>.
*/

//
// Created by hyye on 3/27/18.
//

#include "imu_processor/MeasurementManager.h"

namespace lio {
/*
 * function : set the lio_estimator_node to sub the /imu/data and /compact_data ,and process it 
 */
void MeasurementManager::SetupRos(ros::NodeHandle &nh) {
  is_ros_setup_ = true;

  nh_ = nh;

  sub_imu_ = nh_.subscribe<sensor_msgs::Imu>(mm_config_.imu_topic,
                                             5000,
                                             &MeasurementManager::ImuHandler,
                                             this,
                                             ros::TransportHints().tcpNoDelay());

  sub_compact_data_ = nh_.subscribe<sensor_msgs::PointCloud2>(mm_config_.compact_data_topic,
                                                              100,
                                                              &MeasurementManager::CompactDataHandler,
                                                              this);
//  sub_laser_odom_ =
//      nh_.subscribe<nav_msgs::Odometry>(mm_config_.laser_odom_topic, 10, &MeasurementManager::LaserOdomHandler, this);
}
//imu信息和点云信息
PairMeasurements MeasurementManager::GetMeasurements() {

  PairMeasurements measurements;

  while (true) {

    if (mm_config_.enable_imu) {

      if (imu_buf_.empty() || compact_data_buf_.empty()) {
        return measurements;
      }

      if (imu_buf_.back()->header.stamp.toSec()
          <= compact_data_buf_.front()->header.stamp.toSec() + mm_config_.msg_time_delay) {
        //ROS_DEBUG("wait for imu, only should happen at the beginning");
        // Count for waiting time
        return measurements;
      }

      if (imu_buf_.front()->header.stamp.toSec()
          >= compact_data_buf_.front()->header.stamp.toSec() + mm_config_.msg_time_delay) {
        ROS_DEBUG("throw compact_data, only should happen at the beginning");
        compact_data_buf_.pop();
        continue;
      }
      CompactDataConstPtr compact_data_msg = compact_data_buf_.front();
      compact_data_buf_.pop();

      vector<sensor_msgs::ImuConstPtr> imu_measurements;
      while (imu_buf_.front()->header.stamp.toSec()
          < compact_data_msg->header.stamp.toSec() + mm_config_.msg_time_delay) {
        imu_measurements.emplace_back(imu_buf_.front());
        imu_buf_.pop();
      }

      // NOTE: one message after laser odom msg
      imu_measurements.emplace_back(imu_buf_.front());

      if (imu_measurements.empty()) {
        ROS_DEBUG("no imu between two image");
      }
      measurements.emplace_back(imu_measurements, compact_data_msg);
    } else {
      vector<sensor_msgs::ImuConstPtr> imu_measurements;
      if (compact_data_buf_.empty()) {
        return measurements;
      }
      CompactDataConstPtr compact_data_msg = compact_data_buf_.front();
      compact_data_buf_.pop();
      measurements.emplace_back(imu_measurements, compact_data_msg);
    }

  }

}

/*
 * function: sub the feed_lio /imu/data,store the imu_msg
 */
void MeasurementManager::ImuHandler(const sensor_msgs::ImuConstPtr &raw_imu_msg) {
  if (raw_imu_msg->header.stamp.toSec() <= imu_last_time_) {
    LOG(ERROR) << ("imu message in disorder!");
    return;
  }

  imu_last_time_ = raw_imu_msg->header.stamp.toSec();

  buf_mutex_.lock();
  imu_buf_.push(raw_imu_msg);
  buf_mutex_.unlock();

  con_.notify_one();

  {
    lock_guard<mutex> lg(state_mutex_);
    // TODO: is it necessary to do predict here?
//    Predict(raw_imu_msg);

//    std_msgs::Header header = imu_msg->header;
//    header.frame_id = "world";
//    if (flag == INITIALIZED)
//      PubLatestOdometry(states, header);
  }
} // ImuHandler
//雷达信息处理
void MeasurementManager::LaserOdomHandler(const nav_msgs::OdometryConstPtr &laser_odom_msg) {
  buf_mutex_.lock();
  laser_odom_buf_.push(laser_odom_msg);
  buf_mutex_.unlock();
  con_.notify_one();
}

/*
 * function : get the lio_estimator_node message /compact_data , store it  
 *    在pointodometry中发布
 * 用于高频建图和低频建图的处理
 */
void MeasurementManager::CompactDataHandler(const sensor_msgs::PointCloud2ConstPtr &compact_data_msg) {
  buf_mutex_.lock();
  compact_data_buf_.push(compact_data_msg);
  buf_mutex_.unlock();
  con_.notify_one();
}

}