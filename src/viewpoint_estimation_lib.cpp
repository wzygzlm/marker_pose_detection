/*********************************************************************************************//**
* @file viewpoint_estimation_lib.cpp
*
* ViewPoint Estimator CPP library
*
* Copyright (c)
* Frantisek Durovsky
* Department of Robotics
* Technical University Kosice
* February 2015
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* *********************************************************************************************/
#ifndef VIEWPOINT_ESTIMATOR_LIB_CPP
#define VIEWPOINT_ESTIMATOR_LIB_CPP

////////////////////////////////////////////////////////////////////////////////////////////////

#include <viewpoint_estimation_lib.h>

namespace enc = sensor_msgs::image_encodings;

ViewPoint_Estimator::ViewPoint_Estimator(ros::NodeHandle *nh) :
    square_size(2.75),       //Chessboard square size
    marker_size(0.1),        //Default Marker size in cm
    filename("empty"),       //Initial filename
    camera_frame("world")    //Initial camera frame
{
    nh->getParam("/viewpoint_estimation/calibration_file", filename);
    nh->getParam("/viewpoint_estimation/camera_frame", camera_frame);
    nh->getParam("/viewpoint_estimation/marker_size", marker_size);

    //Marker Visualization with IK
    marker_pub       = nh->advertise<visualization_msgs::Marker>("Estimated_marker", 1);

    load_calibration_file(filename);                //Load camera calibration data
    board_size.width = 9;                           //Set grid parameters
    board_size.height = 6;
    //cv::namedWindow("RGB", CV_WINDOW_AUTOSIZE);

    //Generate chessboard 3D points for solvePnP
    for(int i = 0; i < board_size.height; ++i)
       for(int j = 0; j < board_size.width; ++j)
          chessboard3D_points.push_back(cv::Point3f(float(i*square_size), float(j*square_size), 0.f));

    //Generate reference frame points for chessboard detection
    ref_frame_points.push_back( cv::Point3d( 0.0,  0.0,  0.0 ) );
    ref_frame_points.push_back( cv::Point3d( 10.0, 0.0,  0.0 ) );
    ref_frame_points.push_back( cv::Point3d( 0.0,  10.0, 0.0 ) );
    ref_frame_points.push_back( cv::Point3d( 0.0,  0.0,  10.0) );

}

ViewPoint_Estimator::~ViewPoint_Estimator()
{
    delete intrinsics;
    delete distortion_coeff;
    delete image_size;
}

void
ViewPoint_Estimator::image_callback(const sensor_msgs::ImageConstPtr &original_image)
{
  //ROS Image to Mat structure
  cv_bridge::CvImagePtr cv_ptr;
  try
  {
    cv_ptr = cv_bridge::toCvCopy(original_image, enc::BGR8);
  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("red_ball_detection::cv_bridge_exception %s", e.what());
    return;
  }
  I = cv_ptr->image;

  //============================================
  //Choose Chessboard or Marker detection
  //bool found = chessboard_find_pattern(I,I);
  bool found = markers_find_pattern(I,I);
  //============================================

   //imshow("RGB", I);
   cv::waitKey(10);

}

void ViewPoint_Estimator::image_read_manual(int i)
{
    cv::Mat input_image;
    std::string imageFn;
    imageFn = "/home/minliu/opencv_github/opencv/samples/data/left0" + std::to_string(i+1) + ".jpg";
    input_image = imread(imageFn, cv::IMREAD_GRAYSCALE);
    bool found = chessboard_find_pattern(input_image,input_image);
    imshow("RGB", input_image);
    cv::waitKey(10);
}

bool
ViewPoint_Estimator::chessboard_find_pattern(cv::Mat input_image, cv::Mat output_image)
{
  //Look for feature points in current image
  bool found = cv::findChessboardCorners(input_image, board_size, chessboard2D_points, cv::CALIB_CB_FAST_CHECK );
  //std::cout << "Chessboard detected: " << found << std::endl;
  if (found == true)
  {
      //Estimate chessboard pose
      cv::Mat rvec, tvec;
      cv::solvePnP(cv::Mat(chessboard3D_points), cv::Mat(chessboard2D_points),*intrinsics, *distortion_coeff, rvec, tvec, false);
      //std::cout << "Rvec: " << rvec << std::endl;
      //std::cout << "Tvec: " << tvec << std::endl;

      //Project reference frame on the image
      cv::projectPoints(ref_frame_points, rvec, tvec, *intrinsics, *distortion_coeff, image_frame_points);
      ///////////////////////////////////////////

      //TFs
      cv::Mat R;
      cv::Rodrigues(rvec, R);

      cv::Mat T(4,4, R.type()); //T is 4x4
      T(cv::Range(0,3), cv::Range(0,3)) = R * 1; //copies R into T
      T(cv::Range(0,3), cv::Range(3,4)) = tvec * 1; //copies tvec into T

      //fill the last row of T
      double *p = T.ptr<double>(3);
      p[0] = p[1] = p[2] = 0;
      p[3] = 1;

      //Calibration Grid transform
      tf::Vector3 object_translation(tf::Vector3(tvec.at<double>(0,0)/100, tvec.at<double>(0,1)/100, tvec.at<double>(0,2)/100));
      tf::Matrix3x3 object_rotation(R.at<double>(0,0), R.at<double>(0,1), R.at<double>(0,2),
                                    R.at<double>(1,0), R.at<double>(1,1), R.at<double>(1,2),
                                    R.at<double>(2,0), R.at<double>(2,1), R.at<double>(2,2));

      tf::Transform object_transform(object_rotation, object_translation);
      static tf::TransformBroadcaster br;
      br.sendTransform(tf::StampedTransform(object_transform, ros::Time::now(), "world", "calibration_grid"));

      //Camera transform
      tf::Vector3 camera_translation(0,0,0);
      tf::Matrix3x3 camera_rotation(1,0,0,
                                    0,1,0,
                                    0,0,1);
      
      tf::Transform camera_transform(camera_rotation, camera_translation);
      br.sendTransform(tf::StampedTransform(camera_transform, ros::Time::now(), "world", "camera"));

      //Drawing
      cv::Point p1, p2, p3;
      p1.x = 10; p1.y = 10;
      p2.x = 60; p2.y = 10;
      p3.x = 10; p3.y = 60;
      cv::line(output_image, p1,p2, CV_RGB(255,0,0));
      cv::line(output_image, p1,p3, CV_RGB(0,255,0));
      cv::line(output_image, image_frame_points[0], image_frame_points[1], CV_RGB(255,0,0), 2);
      cv::line(output_image, image_frame_points[0], image_frame_points[2], CV_RGB(0,255,0), 2);
      cv::line(output_image, image_frame_points[0], image_frame_points[3], CV_RGB(0,0,255), 2);
  }
  return(found);
}


bool
ViewPoint_Estimator::markers_find_pattern(cv::Mat input_image,cv::Mat output_image)
{
  aruco::MarkerDetector MDetector;
  std::vector<aruco::Marker> markers;
  static tf::TransformBroadcaster br;

  MDetector.detect(input_image, markers, aruco_calib_params, marker_size);

  for(size_t i = 0; i < markers.size(); i++)
  {
    int current_marker_id = markers[i].id;
    //Draw marker convex, ID, cube and axis
    markers[i].draw(output_image, cv::Scalar(0,0,255), 2);
    aruco::CvDrawingUtils::draw3dCube(output_image, markers[i], aruco_calib_params);
    aruco::CvDrawingUtils::draw3dAxis(output_image, markers[i], aruco_calib_params);

    //Transform marker pose to TFformat
    tf::Transform object_transform = arucoMarker2Tf(markers[i]);
    
    //Marker ID to string
    std::stringstream marker_id_string;
    marker_id_string << "marker_" << current_marker_id;
    //Publish Current Marker TF
    br.sendTransform(tf::StampedTransform(object_transform, ros::Time::now(), camera_frame, marker_id_string.str()));

    //=================================================
    //Publish Current Marker to RViz
    //=================================================
    geometry_msgs::Pose marker_pose_data;
    
    const tf::Vector3 marker_origin = object_transform.getOrigin();
    marker_pose_data.position.x = marker_origin.getX();
    marker_pose_data.position.y = marker_origin.getY();
    marker_pose_data.position.z = marker_origin.getZ();
    
    tf::Quaternion marker_quaternion = object_transform.getRotation();
    marker_pose_data.orientation.x = marker_quaternion.getX();
    marker_pose_data.orientation.y = marker_quaternion.getY();
    marker_pose_data.orientation.z = marker_quaternion.getZ();
    marker_pose_data.orientation.w = marker_quaternion.getW();
 
    publish_marker(marker_pose_data, current_marker_id);

  }
  
  //Display camera frame
  tf::Vector3 camera_translation(0,0,0);
  tf::Matrix3x3 camera_rotation(1,0,0,
                                0,1,0,
                                0,0,1);
 
  tf::Transform camera_transform(camera_rotation, camera_translation);
  br.sendTransform(tf::StampedTransform(camera_transform, ros::Time::now(), camera_frame, "camera"));
}

tf::Transform
ViewPoint_Estimator::arucoMarker2Tf(const aruco::Marker &marker)
{
  cv::Mat marker_rotation(3,3, CV_32FC1);
  cv::Rodrigues(marker.Rvec, marker_rotation);
  cv::Mat marker_translation = marker.Tvec;

  cv::Mat rotate_to_ros(3,3,CV_32FC1);
  rotate_to_ros.at<float>(0,0) = -1.0;
  rotate_to_ros.at<float>(0,1) = 0;
  rotate_to_ros.at<float>(0,2) = 0;
  rotate_to_ros.at<float>(1,0) = 0;
  rotate_to_ros.at<float>(1,1) = 0;
  rotate_to_ros.at<float>(1,2) = 1.0;
  rotate_to_ros.at<float>(2,0) = 0.0;
  rotate_to_ros.at<float>(2,1) = 1.0;
  rotate_to_ros.at<float>(2,2) = 0.0;

  marker_rotation = marker_rotation * rotate_to_ros.t();

  //Origin solution
  tf::Matrix3x3 marker_tf_rot(marker_rotation.at<float>(0,0), marker_rotation.at<float>(0,1), marker_rotation.at<float>(0,2),
                              marker_rotation.at<float>(1,0), marker_rotation.at<float>(1,1), marker_rotation.at<float>(1,2),
                              marker_rotation.at<float>(2,0), marker_rotation.at<float>(2,1), marker_rotation.at<float>(2,2));

  tf::Vector3 marker_tf_tran(marker_translation.at<float>(0,0),
                             marker_translation.at<float>(1,0),
                             marker_translation.at<float>(2,0));

  //Swap X,Y axis rotation
  tf::Quaternion marker_initial_quaternion;
  marker_tf_rot.getRotation(marker_initial_quaternion);

  tf::Quaternion marker_swap_quaternion;
  marker_swap_quaternion.setX(marker_initial_quaternion.getY());
  marker_swap_quaternion.setY(-marker_initial_quaternion.getX());
  marker_swap_quaternion.setZ(marker_initial_quaternion.getZ());
  marker_swap_quaternion.setW(marker_initial_quaternion.getW());

  marker_tf_rot.setRotation(marker_swap_quaternion);

  //Swap X,Y position
 tf::Vector3 marker_swap_pose;

 marker_swap_pose.setX(marker_tf_tran.getY());
 marker_swap_pose.setY(marker_tf_tran.getX());
 marker_swap_pose.setZ(marker_tf_tran.getZ());

 marker_tf_tran.setX(marker_swap_pose.getX());
 marker_tf_tran.setY(marker_swap_pose.getY());
 marker_tf_tran.setZ(marker_swap_pose.getZ());

 return tf::Transform(marker_tf_rot, marker_tf_tran);
}

void ViewPoint_Estimator::publish_marker(geometry_msgs::Pose marker_pose, int marker_id)
{
    visualization_msgs::Marker marker;
    marker.header.frame_id = camera_frame;
    marker.header.stamp = ros::Time::now();
    marker.ns = "basic_shapes";
    marker.id = marker_id;
    marker.type = visualization_msgs::Marker::CUBE;
    marker.action = visualization_msgs::Marker::ADD;

    marker.pose = marker_pose;
    marker.scale.x = 0.1;
    marker.scale.y = 0.1;
    marker.scale.z = 0.01;

    marker.color.r = 0.0f;
    marker.color.g = 1.0f;
    marker.color.b = 0.0f;
    marker.color.a = 1.0;

    marker.lifetime = ros::Duration(0.1);

    //Publish marker
    marker_pub.publish(marker);

}

bool ViewPoint_Estimator::load_calibration_file(std::string filename)
{
  std::cout << "Reading calibration file from: " << filename << std::endl;
  try
  {
      //Searching camera matrix and distortion in calibration textfile
      //# oST version 5.0 parameters
      string camera_matrix_str("camera matrix");
      string distortion_str("distortion");

      ifstream file;
      file.open(filename.c_str());

      intrinsics = new(cv::Mat)(3,3,CV_64F);
      distortion_coeff = new(cv::Mat)(5,1,CV_64F);
      image_size = new(cv::Size);

      std::string line;
      int line_counter = 0;
      while(getline(file, line))
      {
          if(line == camera_matrix_str)
          {
              for(size_t i = 0; i < 3; i++)
                  for(size_t j = 0; j < 3; j++)
                     file >> intrinsics->at<double>(i,j);

		  std::cout << "Intrinsics:" << std::endl << *intrinsics << std::endl;
          }
          if(line == distortion_str)
          {
              for(size_t i = 0; i < 5; i++)
                 file >> distortion_coeff->at<double>(i,0);

              std::cout << "Distortion: " << *distortion_coeff << std::endl;
          }
          line_counter++;
      }

      aruco_calib_params.setParams(*intrinsics, *distortion_coeff, *image_size);

      if ((intrinsics->at<double>(2,2) == 1) && (distortion_coeff->at<double>(0,4) == 0))
          ROS_INFO_STREAM("Calibration file loaded successfully");
      else
	  ROS_WARN("WARNING: Suspicious calibration data");
	
  }
  catch(int e)
  {
     std::cout << "An exception n." << e << "occured";
  }
}

#endif //VIEWPOINT_ESTIMATOR_LIB_CPP
