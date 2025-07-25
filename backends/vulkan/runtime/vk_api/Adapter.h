/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// @lint-ignore-every CLANGTIDY facebook-hte-BadMemberName

#include <executorch/backends/vulkan/runtime/vk_api/vk_api.h>

#include <executorch/backends/vulkan/runtime/vk_api/Device.h>
#include <executorch/backends/vulkan/runtime/vk_api/Pipeline.h>

#include <executorch/backends/vulkan/runtime/vk_api/memory/Allocator.h>

#include <array>

namespace vkcompute {
namespace vkapi {

//
// A Vulkan Adapter represents a logical device and all its properties. It
// manages all relevant properties of the underlying physical device, a
// handle to the logical device, and a number of compute queues available to
// the device. It is primarily responsible for managing the VkDevice handle
// which points to the logical device object on the GPU.
//
// This class is primarily used by the Runtime class, which holds one Adapter
// instance for each physical device visible to the VkInstance. Upon
// construction, this class will populate the physical device properties, but
// will not create the logical device until specifically requested via the
// init_device() function.
//
// init_device() will create the logical device and obtain the VkDevice handle
// for it. It will also create a number of compute queues up to the amount
// requested when the Adapter instance was constructed.
//
// Contexts (which represent one thread of execution) will request a compute
// queue from an Adapter. The Adapter will then select a compute queue to
// assign to the Context, attempting to balance load between all available
// queues. This will allow different Contexts (which typically execute on
// separate threads) to run concurrently.
//

#define NUM_QUEUE_MUTEXES 4

class Adapter final {
 public:
  explicit Adapter(
      VkInstance instance,
      PhysicalDevice physical_device,
      const uint32_t num_queues,
      const std::string& cache_data_path);

  explicit Adapter(
      VkInstance instance,
      VkPhysicalDevice physical_device,
      VkDevice logical_device,
      const uint32_t num_queues,
      const std::string& cache_data_path);

  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;

  Adapter(Adapter&&) = delete;
  Adapter& operator=(Adapter&&) = delete;

  ~Adapter();

  struct Queue {
    uint32_t family_index;
    uint32_t queue_index;
    VkQueueFlags capabilities;
    VkQueue handle;
  };

 private:
  // Use a mutex to manage queue usage info since
  // it can be accessed from multiple threads
  std::mutex queue_usage_mutex_;
  // Physical Device Info
  PhysicalDevice physical_device_;
  // Queue Management
  std::vector<Queue> queues_;
  std::vector<uint32_t> queue_usage_;
  std::array<std::mutex, NUM_QUEUE_MUTEXES> queue_mutexes_;
  // Handles
  VkInstance instance_;
  DeviceHandle device_;
  // Device-level resource caches
  ShaderLayoutCache shader_layout_cache_;
  ShaderCache shader_cache_;
  PipelineLayoutCache pipeline_layout_cache_;
  ComputePipelineCache compute_pipeline_cache_;
  // Memory Management
  SamplerCache sampler_cache_;
  Allocator vma_;
  // Miscellaneous
  bool linear_tiling_3d_enabled_;
  bool owns_device_;

 public:
  // Physical Device metadata

  inline VkPhysicalDevice physical_handle() const {
    return physical_device_.handle;
  }

  inline VkDevice device_handle() const {
    return device_.handle;
  }

  inline bool has_unified_memory() const {
    return physical_device_.has_unified_memory;
  }

  inline uint32_t num_compute_queues() const {
    return physical_device_.num_compute_queues;
  }

  inline bool timestamp_compute_and_graphics() const {
    return physical_device_.has_timestamps;
  }

  inline float timestamp_period() const {
    return physical_device_.timestamp_period;
  }

  // Device Identity
  inline const std::string& device_name() const {
    return physical_device_.device_name;
  }

  inline vkapi::DeviceType device_type() const {
    return physical_device_.device_type;
  }

  // Queue Management

  Queue request_queue();
  void return_queue(Queue&);

  // Caches

  inline ShaderLayoutCache& shader_layout_cache() {
    return shader_layout_cache_;
  }

  inline ShaderCache& shader_cache() {
    return shader_cache_;
  }

  inline PipelineLayoutCache& pipeline_layout_cache() {
    return pipeline_layout_cache_;
  }

  inline ComputePipelineCache& compute_pipeline_cache() {
    return compute_pipeline_cache_;
  }

  // Memory Allocation

  inline SamplerCache& sampler_cache() {
    return sampler_cache_;
  }

  inline Allocator& vma() {
    return vma_;
  }

  inline bool linear_tiling_3d_enabled() const {
    return linear_tiling_3d_enabled_;
  }

  // Physical Device Features

  inline bool supports_16bit_storage_buffers() {
#ifdef VK_KHR_16bit_storage
    return physical_device_.shader_16bit_storage.storageBuffer16BitAccess ==
        VK_TRUE;
#else
    return false;
#endif /* VK_KHR_16bit_storage */
  }

  inline bool supports_8bit_storage_buffers() {
#ifdef VK_KHR_8bit_storage
    return physical_device_.shader_8bit_storage.storageBuffer8BitAccess ==
        VK_TRUE;
#else
    return false;
#endif /* VK_KHR_8bit_storage */
  }

  inline bool supports_float16_shader_types() {
#ifdef VK_KHR_shader_float16_int8
    return physical_device_.shader_float16_int8_types.shaderFloat16 == VK_TRUE;
#else
    return false;
#endif /* VK_KHR_shader_float16_int8 */
  }

  inline bool supports_int8_shader_types() {
#ifdef VK_KHR_shader_float16_int8
    return physical_device_.shader_float16_int8_types.shaderInt8 == VK_TRUE;
#else
    return false;
#endif /* VK_KHR_shader_float16_int8 */
  }

  inline bool supports_int16_shader_types() {
    return physical_device_.supports_int16_shader_types;
  }

  inline bool has_full_float16_buffers_support() {
    return supports_16bit_storage_buffers() && supports_float16_shader_types();
  }

  inline bool has_full_int8_buffers_support() {
    return supports_8bit_storage_buffers() && supports_int8_shader_types();
  }

  inline size_t min_ubo_alignment() const {
    return physical_device_.min_ubo_alignment;
  }

  inline uint32_t max_texture2d_dim() const {
    return physical_device_.properties.limits.maxImageDimension2D;
  }

  inline uint32_t max_texture3d_dim() const {
    return physical_device_.properties.limits.maxImageDimension3D;
  }

  inline uint32_t max_buffer_numel() const {
    return physical_device_.properties.limits.maxStorageBufferRange;
  }

  // Command Buffer Submission

  void submit_cmd(
      const Queue&,
      VkCommandBuffer,
      VkFence fence = VK_NULL_HANDLE,
      VkSemaphore wait_semaphore = VK_NULL_HANDLE,
      VkSemaphore signal_semaphore = VK_NULL_HANDLE);

  std::string stringize() const;
  friend std::ostream& operator<<(std::ostream&, const Adapter&);
};

} // namespace vkapi
} // namespace vkcompute
