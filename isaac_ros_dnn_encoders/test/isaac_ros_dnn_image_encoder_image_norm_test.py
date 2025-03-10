# SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
# Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

from math import floor
import os
import pathlib
import struct
import time

from cv_bridge import CvBridge
from isaac_ros_tensor_list_interfaces.msg import TensorList
from isaac_ros_test import IsaacROSBaseTest
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
import numpy as np

import pytest
import rclpy

from sensor_msgs.msg import Image


DIMENSION_WIDTH = 100
DIMENSION_HEIGHT = 100


@pytest.mark.rostest
def generate_test_description():
    encoder_node = ComposableNode(
        name='encoder',
        package='isaac_ros_dnn_encoders',
        plugin='nvidia::isaac_ros::dnn_inference::DnnImageEncoderNode',
        namespace=IsaacROSDnnImageEncoderImageNormNodeTest.generate_namespace(),
        parameters=[{
            'network_image_width': DIMENSION_WIDTH,
            'network_image_height': DIMENSION_HEIGHT,
            'image_mean': [0.5, 0.6, 0.25],
            'image_stddev': [0.25, 0.8, 0.5]
        }],
        remappings=[('encoded_tensor', 'tensors')])

    return IsaacROSDnnImageEncoderImageNormNodeTest.generate_test_description([
        ComposableNodeContainer(
            name='tensor_rt_container',
            package='rclcpp_components',
            executable='component_container_mt',
            composable_node_descriptions=[encoder_node],
            namespace=IsaacROSDnnImageEncoderImageNormNodeTest.generate_namespace(),
            output='screen',
            arguments=['--ros-args', '--log-level', 'info',
                       '--log-level', 'isaac_ros_test.encoder:=debug'],
        )
    ])


class IsaacROSDnnImageEncoderImageNormNodeTest(IsaacROSBaseTest):
    filepath = pathlib.Path(os.path.dirname(__file__))

    def test_image_normalization(self):
        """
        Test Image Normalization feature.

        Test that the DNN Image encoder is correctly normalizing the image based on
        the given image mean and standard deviation vectors.
        Given that the image mean vector is <0.5, 0.6, 0.25>, and the image standard
        deviation vector is <0.25, 0.8, 0.5>, and that our input image is white
        (each pixel value is 255), the value for each channel should be:
        RED: ((255 / 255) - 0.5) / 0.25 = 2.0
        GREEN: ((255 / 255) - 0.6) / 0.8 = 0.5
        BLUE: ((255/ 255) - 0.25) / 0.5 = 1.5
        This test verifies that each channel's values should be the calculated values
        above.
        """
        TIMEOUT = 300
        received_messages = {}
        RED_EXPECTED_VAL = 2.0
        GREEN_EXPECTED_VAL = 0.5
        BLUE_EXPECTED_VAL = 1.5

        self.generate_namespace_lookup(['image', 'tensors'])

        image_pub = self.node.create_publisher(
            Image, self.namespaces['image'], self.DEFAULT_QOS)

        subs = self.create_logging_subscribers(
            [('tensors', TensorList)], received_messages)

        try:
            # Create white image
            cv_image = np.zeros((DIMENSION_HEIGHT, DIMENSION_WIDTH, 3), np.uint8)
            cv_image[:] = (255, 255, 255)
            image = CvBridge().cv2_to_imgmsg(cv_image)
            image.encoding = 'bgr8'

            end_time = time.time() + TIMEOUT
            done = False

            while time.time() < end_time:
                image_pub.publish(image)
                rclpy.spin_once(self.node, timeout_sec=(0.1))
                if 'tensors' in received_messages:
                    done = True
                    break
            self.assertTrue(done, 'Appropriate output not received')
            tensor = received_messages['tensors'].tensors[0]

            # This tensor has the format NCHW, so the stride for channel is
            # calculated by VALUES_PER_CHANNEL. `tensor.data` is also storing
            # raw bytes - the tensor values are floats, which are 4 bytes.
            VALUES_PER_CHANNEL = DIMENSION_HEIGHT * DIMENSION_WIDTH
            SIZEOF_FLOAT = 4

            for i in range(0, floor(len(tensor.data) / SIZEOF_FLOAT)):
                # struct.unpack returns a tuple with one element
                result_val = struct.unpack(
                    '<f', tensor.data[SIZEOF_FLOAT * i: SIZEOF_FLOAT * i + SIZEOF_FLOAT])[0]
                if i // VALUES_PER_CHANNEL == 0:  # Red
                    self.assertTrue(round(result_val, 1) == RED_EXPECTED_VAL)
                elif i // VALUES_PER_CHANNEL == 1:  # Green
                    self.assertTrue(round(result_val, 1) == GREEN_EXPECTED_VAL)
                else:  # Blue
                    self.assertTrue(round(result_val, 1) == BLUE_EXPECTED_VAL)

        finally:
            self.node.destroy_subscription(subs)
            self.node.destroy_publisher(image_pub)
