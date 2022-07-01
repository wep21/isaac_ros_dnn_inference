# Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

import launch
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    """Load and launch mobilenetv2-1.0 onnx model through Triton node."""
    launch_args = [
        DeclareLaunchArgument(
            'model_name',
            default_value='',
            description='The name of the model'),
        DeclareLaunchArgument(
            'model_repository_paths',
            default_value='',
            description='The absolute path to the repository of models'),
        DeclareLaunchArgument(
            'max_batch_size',
            default_value='0',
            description='The maximum allowed batch size of the model'),
        DeclareLaunchArgument(
            'input_tensor_names',
            default_value='["input"]',
            description='A list of tensor names to bound to the specified input binding names'),
        DeclareLaunchArgument(
            'input_binding_names',
            default_value='["data"]',
            description='A list of input tensor binding names (specified by model)'),
        DeclareLaunchArgument(
            'output_tensor_names',
            default_value='["output"]',
            description='A list of tensor names to bound to the specified output binding names'),
        DeclareLaunchArgument(
            'output_binding_names',
            default_value='["mobilenetv20_output_flatten0_reshape0"]',
            description='A  list of output tensor binding names (specified by model)'),
    ]

    # Triton parameters
    model_name = LaunchConfiguration('model_name')
    model_repository_paths = LaunchConfiguration('model_repository_paths')
    max_batch_size = LaunchConfiguration('max_batch_size')
    input_tensor_names = LaunchConfiguration('input_tensor_names')
    input_binding_names = LaunchConfiguration('input_binding_names')
    output_tensor_names = LaunchConfiguration('output_tensor_names')
    output_binding_names = LaunchConfiguration('output_binding_names')

    triton_node = ComposableNode(
        name='triton',
        package='isaac_ros_triton',
        plugin='nvidia::isaac_ros::dnn_inference::TritonNode',
        parameters=[{
            'model_name': model_name,
            'model_repository_paths': model_repository_paths,
            'max_batch_size': max_batch_size,
            'input_binding_names': input_binding_names,
            'output_binding_names': output_binding_names,
            'input_tensor_names': input_tensor_names,
            'output_tensor_names': output_tensor_names
        }]
    )

    triton_container = ComposableNodeContainer(
        name='triton_container',
        namespace='triton',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[triton_node],
        output='screen'
    )

    final_launch_description = launch_args + [triton_container]
    return launch.LaunchDescription(final_launch_description)
