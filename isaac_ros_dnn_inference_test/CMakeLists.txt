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

cmake_minimum_required(VERSION 3.5)
project(isaac_ros_dnn_inference_test LANGUAGES C CXX)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Default to Release build
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()
message( STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}" )

execute_process(COMMAND uname -m COMMAND tr -d '\n'
  OUTPUT_VARIABLE ARCHITECTURE
)
message( STATUS "Architecture: ${ARCHITECTURE}" )

find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

# test_tensor_publisher_node
ament_auto_add_library(test_tensor_publisher_node SHARED src/test_tensor_publisher_node.cpp)
target_compile_definitions(test_tensor_publisher_node
  PRIVATE "COMPOSITION_BUILDING_DLL"
)
target_link_libraries(test_tensor_publisher_node)
rclcpp_components_register_nodes(test_tensor_publisher_node "nvidia::isaac_ros::dnn_inference::TestTensorPublisherNode")
set(node_plugins "${node_plugins}nvidia::isaac_ros::dnn_inference::TestTensorPublisherNode;$<TARGET_FILE:test_tensor_publisher_node>\n")

# run test tensor publisher executable
ament_auto_add_executable("run_test_publisher"
  src/test_tensor_publisher_main.cpp
)

target_link_libraries("run_test_publisher" test_tensor_publisher_node)

install(TARGETS "run_test_publisher"
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

endif()

ament_auto_package()
