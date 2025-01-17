﻿// File: geometry.hpp
// Project: core
// Created Date: 14/04/2021
// Author: Shun Suzuki
// -----
// Last Modified: 10/10/2021
// Modified By: Shun Suzuki (suzuki@hapis.k.u-tokyo.ac.jp)
// -----
// Copyright (c) 2021 Hapis Lab. All rights reserved.
//

#pragma once

#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6031 6255 6294 26450 26451 26454 26495 26812)
#endif
#if defined(__GNUC__) && !defined(__llvm__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <Eigen/Dense>
#if _MSC_VER
#pragma warning(pop)
#endif
#if defined(__GNUC__) && !defined(__llvm__)
#pragma GCC diagnostic pop
#endif

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "hardware_defined.hpp"

namespace autd {
namespace core {

class Geometry;
using GeometryPtr = std::unique_ptr<Geometry>;

using Vector3 = Eigen::Matrix<double, 3, 1>;
using Vector4 = Eigen::Matrix<double, 4, 1>;
using Matrix4X4 = Eigen::Matrix<double, 4, 4>;
using Quaternion = Eigen::Quaternion<double>;

/**
 * \brief Device contains an AUTD device geometry.
 */
struct Device {
  Device(const Vector3& position, const Quaternion& quaternion, const uint16_t freq_cycle)
      : freq_cycle(freq_cycle),
        x_direction(quaternion * Vector3(1, 0, 0)),
        y_direction(quaternion * Vector3(0, 1, 0)),
        z_direction(quaternion * Vector3(0, 0, 1)),
        global_trans_positions(std::make_unique<Vector3[]>(NUM_TRANS_IN_UNIT)) {
    const Eigen::Transform<double, 3, Eigen::Affine> transform_matrix = Eigen::Translation<double, 3>(position) * quaternion;
    auto index = 0;
    for (size_t y = 0; y < NUM_TRANS_Y; y++)
      for (size_t x = 0; x < NUM_TRANS_X; x++) {
        if (is_missing_transducer(x, y)) continue;
        const Vector4 local_pos = Vector4(static_cast<double>(x) * TRANS_SPACING_MM, static_cast<double>(y) * TRANS_SPACING_MM, 0, 1);
        const Vector4 global_pos = transform_matrix * local_pos;
        global_trans_positions[index++] = Vector3(global_pos[0], global_pos[1], global_pos[2]);
      }
    g2l = transform_matrix.inverse();
  }

  Device(const Vector3& position, const Vector3& euler_angles, const uint16_t freq_cycle)
      : Device(position,
               Eigen::AngleAxis<double>(euler_angles.x(), Vector3::UnitZ()) * Eigen::AngleAxis<double>(euler_angles.y(), Vector3::UnitY()) *
                   Eigen::AngleAxis<double>(euler_angles.z(), Vector3::UnitZ()),
               freq_cycle) {}

  uint16_t freq_cycle;
  Vector3 x_direction;
  Vector3 y_direction;
  Vector3 z_direction;
  std::unique_ptr<Vector3[]> global_trans_positions;
  Eigen::Transform<double, 3, Eigen::Affine> g2l;
};

/**
 * @brief Geometry of all devices
 */
class Geometry {
 public:
  Geometry() : _c(340e3) {}
  ~Geometry() = default;
  Geometry(const Geometry& v) noexcept = default;
  Geometry& operator=(const Geometry& obj) = default;
  Geometry(Geometry&& obj) = default;
  Geometry& operator=(Geometry&& obj) = default;

  /**
   * @brief  Add new device with position and rotation. Note that the transform is done with order: Translate -> Rotate
   * @param position Position of transducer #0, which is the one at the lower-left corner.
   * (The lower-left corner is the one with the two missing transducers.)
   * @param euler_angles ZYZ convention euler angle of the device
   * @param freq_cycle frequency will be  FPGA_BASE_CLK_FREQ / freq_cycle
   * @return an id of added device
   */
  size_t add_device(const Vector3& position, const Vector3& euler_angles, const uint16_t freq_cycle = FPGA_BASE_CLK_FREQ / 40000) {
    const auto device_id = this->_devices.size();
    this->_devices.emplace_back(position, euler_angles, freq_cycle);
    return device_id;
  }

  /**
   * @brief Same as add_device(const Vector3&, const Vector3&, const size_t), but using quaternion rather than zyz euler angles.
   * @param position Position of transducer #0, which is the one at the lower-left corner.
   * @param quaternion rotation quaternion of the device.
   * @param freq_cycle frequency will be  FPGA_BASE_CLK_FREQ / freq_cycle
   * @return an id of added device
   */
  size_t add_device(const Vector3& position, const Quaternion& quaternion, const uint16_t freq_cycle = FPGA_BASE_CLK_FREQ / 40000) {
    const auto device_id = this->_devices.size();
    this->_devices.emplace_back(position, quaternion, freq_cycle);
    return device_id;
  }

  /**
   * @brief Delete device
   * @param idx Index of the device to delete
   * @return an index of deleted device
   */
  size_t del_device(const size_t idx) {
    this->_devices.erase(this->_devices.begin() + idx);
    return idx;
  }

  /**
   * @brief Clear all devices
   */
  void clear_devices() { std::vector<Device>().swap(this->_devices); }

  /**
   * @brief Speed of sound
   */
  double& sound_speed() noexcept { return this->_c; }

  /**
   * @brief ultrasound wavelength
   */
  [[nodiscard]] double wavelength(const size_t dev_idx) const { return this->_c / frequency(dev_idx); }

  /**
   * @brief Number of devices
   */
  [[nodiscard]] size_t num_devices() const noexcept { return this->_devices.size(); }

  /**
   * @brief Number of transducers
   */
  [[nodiscard]] size_t num_transducers() const noexcept { return this->num_devices() * NUM_TRANS_IN_UNIT; }

  [[nodiscard]] uint16_t freq_cycle(const size_t device_idx) const { return this->_devices[device_idx].freq_cycle; }

  [[nodiscard]] double frequency(const size_t device_idx) const {
    return static_cast<double>(FPGA_BASE_CLK_FREQ) / static_cast<double>(freq_cycle(device_idx));
  }

  /**
   * @brief Position of a transducer specified by id
   */
  [[nodiscard]] const Vector3& position(const size_t global_transducer_idx) const {
    const auto local_trans_id = global_transducer_idx % NUM_TRANS_IN_UNIT;
    return position(device_idx_for_trans_idx(global_transducer_idx), local_trans_id);
  }

  /**
   * @brief Position of a transducer specified by id
   */
  [[nodiscard]] const Vector3& position(const size_t device_idx, const size_t local_transducer_idx) const {
    return this->_devices[device_idx].global_trans_positions[local_transducer_idx];
  }

  /**
   * @brief Convert a global position to a local position
   */
  [[nodiscard]] Vector3 to_local_position(const size_t device_idx, const Vector3& global_position) const {
    const Vector4 homo = Vector4(global_position[0], global_position[1], global_position[2], 1);
    const Vector4 local_position = this->_devices[device_idx].g2l * homo;
    return Vector3(local_position[0], local_position[1], local_position[2]);
  }

  /**
   * @brief Normalized direction of a device
   */
  [[nodiscard]] const Vector3& direction(const size_t device_idx) const { return this->_devices[device_idx].z_direction; }

  /**
   * @brief Normalized long-axis direction of a device
   */
  [[nodiscard]] const Vector3& x_direction(const size_t device_idx) const { return this->_devices[device_idx].x_direction; }

  /**
   * @brief Normalized short-axis direction of a device
   */
  [[nodiscard]] const Vector3& y_direction(const size_t device_idx) const { return this->_devices[device_idx].y_direction; }

  /**
   * @brief Same as the direction()
   */
  [[nodiscard]] const Vector3& z_direction(const size_t device_idx) const { return this->_devices[device_idx].z_direction; }

  /**
   * @brief Convert transducer index into device index
   */
  [[nodiscard]] static size_t device_idx_for_trans_idx(const size_t transducer_idx) { return transducer_idx / NUM_TRANS_IN_UNIT; }

 private:
  std::vector<Device> _devices;
  double _c;
};
}  // namespace core
}  // namespace autd
