// SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
// Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "isaac_ros_dnn_encoders/dnn_image_encoder_node.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "isaac_ros_nitros_image_type/nitros_image.hpp"
#include "isaac_ros_nitros_tensor_list_type/nitros_tensor_list.hpp"

namespace nvidia
{
namespace isaac_ros
{
namespace dnn_inference
{

namespace
{
// Map to store the nitros tensor format to nitros image type
const std::unordered_map<std::string, std::string> tensor_to_image_type({
          {"nitros_tensor_list_nchw_rgb_f32", "nitros_image_rgb8"},
          {"nitros_tensor_list_nchw_bgr_f32", "nitros_image_bgr8"}
        });

const std::unordered_map<std::string, int32_t> image_type_to_channel_size({
          {"nitros_image_rgb8", 3},
          {"nitros_image_bgr8", 3}
        });

constexpr double PER_PIXEL_SCALE = 255.0;

constexpr char INPUT_COMPONENT_KEY[] = "broadcaster/data_receiver";
constexpr char INPUT_DEFAULT_IMAGE_FORMAT[] = "nitros_image_bgr8";
constexpr char INPUT_TOPIC_NAME[] = "image";

constexpr char OUTPUT_COMPONENT_KEY[] = "vault/vault";
constexpr char OUTPUT_DEFAULT_TENSOR_FORMAT[] = "nitros_tensor_list_nchw_rgb_f32";
constexpr char OUTPUT_TOPIC_NAME[] = "encoded_tensor";

constexpr char APP_YAML_FILENAME[] = "config/dnn_image_encoder_node.yaml";
constexpr char PACKAGE_NAME[] = "isaac_ros_dnn_encoders";

const std::vector<std::pair<std::string, std::string>> EXTENSIONS = {
  {"isaac_ros_nitros", "gxf/std/libgxf_std.so"},
  {"isaac_ros_nitros", "gxf/cuda/libgxf_cuda.so"},
  {"isaac_ros_nitros", "gxf/serialization/libgxf_serialization.so"},
  {"isaac_ros_nitros", "gxf/tensorops/libgxf_tensorops.so"},
  {"isaac_ros_nitros", "gxf/libgxf_message_compositor.so"}
};
const std::vector<std::string> PRESET_EXTENSION_SPEC_NAMES = {
  "isaac_ros_dnn_encoders",
};
const std::vector<std::string> EXTENSION_SPEC_FILENAMES = {};
const std::vector<std::string> GENERATOR_RULE_FILENAMES = {
  "config/namespace_injector_rule.yaml"
};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
const nitros::NitrosPublisherSubscriberConfigMap CONFIG_MAP = {
  {INPUT_COMPONENT_KEY,
    {
      .type = nitros::NitrosPublisherSubscriberType::NEGOTIATED,
      .qos = rclcpp::QoS(1),
      .compatible_data_format = INPUT_DEFAULT_IMAGE_FORMAT,
      .topic_name = INPUT_TOPIC_NAME
    }
  },
  {OUTPUT_COMPONENT_KEY,
    {
      .type = nitros::NitrosPublisherSubscriberType::NEGOTIATED,
      .qos = rclcpp::QoS(1),
      .compatible_data_format = OUTPUT_DEFAULT_TENSOR_FORMAT,
      .topic_name = OUTPUT_TOPIC_NAME,
      .frame_id_source_key = INPUT_COMPONENT_KEY
    }
  }
};
}  // namespace
#pragma GCC diagnostic pop

DnnImageEncoderNode::DnnImageEncoderNode(const rclcpp::NodeOptions options)
: nitros::NitrosNode(options,
    APP_YAML_FILENAME,
    CONFIG_MAP,
    PRESET_EXTENSION_SPEC_NAMES,
    EXTENSION_SPEC_FILENAMES,
    GENERATOR_RULE_FILENAMES,
    EXTENSIONS,
    PACKAGE_NAME),
  network_image_width_(declare_parameter<uint16_t>("network_image_width", 0)),
  network_image_height_(declare_parameter<uint16_t>("network_image_height", 0)),
  image_mean_(declare_parameter<std::vector<double>>("image_mean", {0.5, 0.5, 0.5})),
  image_stddev_(declare_parameter<std::vector<double>>("image_stddev", {0.5, 0.5, 0.5}))
{
  if (network_image_width_ == 0) {
    throw std::invalid_argument(
            "[Dnn Image Encoder] Invalid network_image_width, "
            "this needs to be set per the model input requirements.");
  }
  if (network_image_height_ == 0) {
    throw std::invalid_argument(
            "[Dnn Image Encoder] Invalid network_image_height_, "
            "this needs to be set per the model input requirements.");
  }

  registerSupportedType<nvidia::isaac_ros::nitros::NitrosImage>();
  registerSupportedType<nvidia::isaac_ros::nitros::NitrosTensorList>();

  startNitrosNode();
}

void DnnImageEncoderNode::preLoadGraphCallback()
{
  RCLCPP_INFO(get_logger(), "In DNN Image Encoder Node preLoadGraphCallback().");

  std::vector<double> scale = {
    1.0 / (PER_PIXEL_SCALE * image_stddev_[0]),
    1.0 / (PER_PIXEL_SCALE * image_stddev_[1]),
    1.0 / (PER_PIXEL_SCALE * image_stddev_[2])};
  std::vector<double> offset = {
    -(PER_PIXEL_SCALE * image_mean_[0]),
    -(PER_PIXEL_SCALE * image_mean_[1]),
    -(PER_PIXEL_SCALE * image_mean_[2])};

  std::string scales = "[" + std::to_string(scale[0]) + "," + \
    std::to_string(scale[1]) + "," + \
    std::to_string(scale[2]) + "]";
  NitrosNode::preLoadGraphSetParameter(
    "normalizer",
    "nvidia::cvcore::tensor_ops::Normalize",
    "scales",
    scales);

  std::string offsets = "[" + std::to_string(offset[0]) + "," + \
    std::to_string(offset[1]) + "," + \
    std::to_string(offset[2]) + "]";
  NitrosNode::preLoadGraphSetParameter(
    "normalizer",
    "nvidia::cvcore::tensor_ops::Normalize",
    "offsets",
    offsets);
}

void DnnImageEncoderNode::postLoadGraphCallback()
{
  RCLCPP_INFO(get_logger(), "In DNN Image Encoder Node postLoadGraphCallback().");

  getNitrosContext().setParameterUInt64(
    "resizer", "nvidia::cvcore::tensor_ops::Resize", "output_width", network_image_width_);
  getNitrosContext().setParameterUInt64(
    "resizer", "nvidia::cvcore::tensor_ops::Resize", "output_height", network_image_height_);

  const gxf::optimizer::ComponentInfo output_comp_info = {
    "nvidia::gxf::Vault",  // component_type_name
    "vault",               // component_name
    "vault"                // entity_name
  };
  const std::string negotiated_tensor_format = getFinalDataFormat(output_comp_info);

  const auto image_type = tensor_to_image_type.find(negotiated_tensor_format);
  if (image_type == std::end(tensor_to_image_type)) {
    RCLCPP_ERROR(
      get_logger(), "Unsupported NITROS tensor type[%s].", negotiated_tensor_format.c_str());
    throw std::runtime_error("Unsupported NITROS tensor type.");
  } else {
    uint64_t block_size = calculate_image_size(
      image_type->second, network_image_width_, network_image_height_);

    RCLCPP_DEBUG(
      get_logger(), "postLoadGraphCallback() block_size = %ld.",
      block_size);

    getNitrosContext().setParameterUInt64(
      "resizer", "nvidia::gxf::BlockMemoryPool", "block_size", block_size);
    getNitrosContext().setParameterUInt64(
      "color_space_converter", "nvidia::gxf::BlockMemoryPool", "block_size", block_size);
    getNitrosContext().setParameterUInt64(
      "normalizer", "nvidia::gxf::BlockMemoryPool", "block_size", block_size * 4);
    getNitrosContext().setParameterUInt64(
      "interleaved_to_planar", "nvidia::gxf::BlockMemoryPool", "block_size", block_size * 4);
    getNitrosContext().setParameterUInt64(
      "reshaper", "nvidia::gxf::BlockMemoryPool", "block_size", block_size * sizeof(float));

    std::vector<int32_t> final_tensor_shape{1,
      static_cast<int32_t>(image_type_to_channel_size.at(image_type->second)),
      static_cast<int32_t>(network_image_height_),
      static_cast<int32_t>(network_image_width_)
    };

    getNitrosContext().setParameter1DInt32Vector(
      "reshaper", "nvidia::cvcore::tensor_ops::Reshape", "output_shape",
      final_tensor_shape);
  }
}

DnnImageEncoderNode::~DnnImageEncoderNode() = default;

}  // namespace dnn_inference
}  // namespace isaac_ros
}  // namespace nvidia

// Register as component
#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(nvidia::isaac_ros::dnn_inference::DnnImageEncoderNode)
