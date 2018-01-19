/*********************************************************************************************//**
* @file viewpoint_estimation.cpp
*
* main ViewPoint Estimator file
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
#include <ros/ros.h>
#include <viewpoint_estimation_lib.h>

int 
main(int argc, char *argv[])
{
  ros::init(argc, argv, "viewpoint_estimation");
  ros::NodeHandle nh;

  ViewPoint_Estimator est(&nh);

  image_transport::ImageTransport it(nh);
  image_transport::Subscriber sub = it.subscribe("/image_raw", 1, &ViewPoint_Estimator::image_callback, &est);

  int i = 0;
  while (ros::ok())
  {
      if (i >= 9)
      {
          i = 0;
      }
      est.image_read_manual(i);
      i++;
  }
  ros::spin();

  return(EXIT_SUCCESS);
}
