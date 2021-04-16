﻿// File: link.hpp
// Project: lib
// Created Date: 01/06/2016
// Author: Seki Inoue
// -----
// Last Modified: 16/04/2021
// Modified By: Shun Suzuki (suzuki@hapis.k.u-tokyo.ac.jp)
// -----
// Copyright (c) 2016-2020 Hapis Lab. All rights reserved.
//

#pragma once

#include <memory>
#include <string>

#include "result.hpp"

namespace autd {
/**
 * @brief Link is the interface to the AUTD device
 */
class Link {
 public:
  Link() = default;
  virtual ~Link() = default;
  Link(const Link& v) = delete;
  Link& operator=(const Link& obj) = delete;
  Link(Link&& obj) = delete;
  Link& operator=(Link&& obj) = delete;

  /**
   * @brief Open link
   * @return return Ok(whether succeeded to open), or Err(error msg) if some unrecoverable error occurred
   */
  [[nodiscard]] virtual Result<bool, std::string> Open() = 0;

  /**
   * @brief Close link
   * @return return Ok(whether succeeded to close), or Err(error msg) if some unrecoverable error occurred
   */
  [[nodiscard]] virtual Result<bool, std::string> Close() = 0;

  /**
   * @brief  Send data to devices
   * @return return Ok(whether succeeded to send), or Err(error msg) if some unrecoverable error occurred
   */
  [[nodiscard]] virtual Result<bool, std::string> Send(size_t size, const uint8_t* buf) = 0;

  /**
   * @brief  Read data from devices
   * @return return Ok(whether succeeded to read), or Err(error msg) if some unrecoverable error occurred
   */
  [[nodiscard]] virtual Result<bool, std::string> Read(uint8_t* rx, size_t buffer_len) = 0;
  [[nodiscard]] virtual bool is_open() = 0;
};

using LinkPtr = std::unique_ptr<Link>;
}  // namespace autd
