/*
 * Copyright (C) 2016 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <functional>
#include <mutex>

#include <gtest/gtest.h>

#include "gazebo/test/ServerFixture.hh"

using namespace gazebo;
class DepthCameraSensor_TEST : public ServerFixture
{
};

std::mutex g_depthMutex;
unsigned int g_depthCounter = 0;
float *g_depthBuffer = nullptr;

/////////////////////////////////////////////////
void OnNewDepthFrame(const float * _image,
    unsigned int _width, unsigned int _height,
    unsigned int /*_depth*/, const std::string &/*_format*/)
{
  if (!_image)
    return;
  std::lock_guard<std::mutex> lock(g_depthMutex);
  if (!g_depthBuffer)
    g_depthBuffer = new float[_width * _height];
  memcpy(g_depthBuffer,  _image, _width * _height * sizeof(_image[0]));
  g_depthCounter++;
}

/////////////////////////////////////////////////
/// \brief Test Creation of a Depth Camera sensor
TEST_F(DepthCameraSensor_TEST, CreateDepthCamera)
{
  Load("worlds/depth_camera.world");
  sensors::SensorManager *mgr = sensors::SensorManager::Instance();

  // Create the camera sensor
  std::string sensorName = "default::camera_model::my_link::camera";

  // Get a pointer to the depth camera sensor
  sensors::DepthCameraSensorPtr sensor =
     std::dynamic_pointer_cast<sensors::DepthCameraSensor>
     (mgr->GetSensor(sensorName));

  // Make sure the above dynamic cast worked.
  EXPECT_TRUE(sensor != nullptr);

  EXPECT_EQ(sensor->ImageWidth(), 640u);
  EXPECT_EQ(sensor->ImageHeight(), 480u);
  EXPECT_TRUE(sensor->IsActive());

  rendering::DepthCameraPtr depthCamera = sensor->DepthCamera();
  EXPECT_TRUE(depthCamera != nullptr);

  event::ConnectionPtr c = depthCamera->ConnectNewDepthFrame(
      std::bind(&::OnNewDepthFrame, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
      std::placeholders::_5));

  // wait for a few depth camera frames
  int i = 0;
  double updateRate = sensor->UpdateRate();
  EXPECT_DOUBLE_EQ(10.0, updateRate);
  while (i < 300)
  {
    common::Time::MSleep(10);
    i++;
  }
  EXPECT_GE(i, 3 * updateRate);

  unsigned int imageSize =
      sensor->ImageWidth() * sensor->ImageHeight();

  std::lock_guard<std::mutex> lock(g_depthMutex);
  // Check that the depth values are valid
  for (unsigned int i = 0; i < imageSize; ++i)
  {
    EXPECT_TRUE(g_depthBuffer[i] <= depthCamera->FarClip());
    EXPECT_TRUE(g_depthBuffer[i] >= depthCamera->NearClip());
    EXPECT_TRUE(!ignition::math::equal(g_depthBuffer[i], 0.0f));
  }

  // sphere with radius 1m is at 2m in front of depth camera
  // so verify depth readings are between 1-2m in the mid row
  unsigned int halfHeight =
    static_cast<unsigned int>(sensor->ImageHeight()*0.5)-1;
  for (unsigned int i = sensor->ImageWidth()*halfHeight;
      i < sensor->ImageWidth()*(halfHeight+1); ++i)
  {
    EXPECT_TRUE(g_depthBuffer[i] < 2.0f);
    EXPECT_TRUE(g_depthBuffer[i] >= 1.0f);
  }

  if (g_depthBuffer)
    delete [] g_depthBuffer;
}


using namespace gazebo;
class DepthCameraSensor_normals_TEST : public ServerFixture
{
};

unsigned int g_normalsCounter = 0;

/////////////////////////////////////////////////
void OnNewNormalsFrame(const float * _normals,
                       unsigned int _width,
                       unsigned int _height,
                       unsigned int /*_depth*/,
                       const std::string &/*_format*/)
{
  for (unsigned int i = 0; i < _width; i++)
  {
    for (unsigned int j = 0; j < _height; j++)
    {
      unsigned int index = (j * _width) + i;
      float x = _normals[4 * index];
      float y = _normals[4 * index + 1];
      float z = _normals[4 * index + 2];
      // float a = _normals[4 * index + 3];
      if (x > 0.0 && y > 0.0 && z > 0.0)
      {
        EXPECT_NEAR(x, 0.0, 0.01);
        EXPECT_NEAR(y, 0.0, 0.01);
        EXPECT_NEAR(z, -1.0, 0.01);
      }
    }
  }
  g_normalsCounter++;
}

/////////////////////////////////////////////////
/// \brief Test Creation of a Depth Camera sensor
TEST_F(DepthCameraSensor_normals_TEST, CreateDepthCamera)
{
  Load("worlds/depth_camera2.world");
  sensors::SensorManager *mgr = sensors::SensorManager::Instance();

  // Create the camera sensor
  std::string sensorName = "default::camera_model::my_link::camera";

  // Get a pointer to the depth camera sensor
  sensors::DepthCameraSensorPtr sensor =
     std::dynamic_pointer_cast<sensors::DepthCameraSensor>
     (mgr->GetSensor(sensorName));

  // Make sure the above dynamic cast worked.
  EXPECT_TRUE(sensor != nullptr);

  EXPECT_EQ(sensor->ImageWidth(), 640u);
  EXPECT_EQ(sensor->ImageHeight(), 480u);
  EXPECT_TRUE(sensor->IsActive());

  rendering::DepthCameraPtr depthCamera = sensor->DepthCamera();
  EXPECT_TRUE(depthCamera != nullptr);

  event::ConnectionPtr c2 = depthCamera->ConnectNewNormalsPointCloud(
      std::bind(&::OnNewNormalsFrame, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
      std::placeholders::_5));

  double updateRate = sensor->UpdateRate();
  EXPECT_DOUBLE_EQ(10.0, updateRate);
  // wait for a few depth camera frames
  int i = 0;
  while (i < 300)
  {
    common::Time::MSleep(10);
    i++;
  }
  EXPECT_GE(g_normalsCounter, 3 * updateRate);
}

/////////////////////////////////////////////////
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
