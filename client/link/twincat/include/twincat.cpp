﻿// File: ethercat_link.cpp
// Project: lib
// Created Date: 01/06/2016
// Author: Seki Inoue
// -----
// Last Modified: 15/04/2021
// Modified By: Shun Suzuki (suzuki@hapis.k.u-tokyo.ac.jp)
// -----
// Copyright (c) 2016-2020 Hapis Lab. All rights reserved.
//

#include "twincat.hpp"

#if _WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <winnt.h>
#else
typedef void* HMODULE;
#endif

#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace autd::link {

constexpr uint32_t INDEX_GROUP = 0x3040030;
constexpr uint32_t INDEX_OFFSET_BASE = 0x81000000;
constexpr uint32_t INDEX_OFFSET_BASE_READ = 0x80000000;
constexpr uint16_t PORT = 301;

static std::vector<std::string> Split(const std::string& s, const char deliminator) {
  std::vector<std::string> tokens;
  std::string token;
  for (auto ch : s) {
    if (ch == deliminator) {
      if (!token.empty()) tokens.emplace_back(token);
      token.clear();
    } else {
      token += ch;
    }
  }
  if (!token.empty()) tokens.emplace_back(token);
  return tokens;
}

class LocalTwinCATLink : Link {
 public:
  LocalTwinCATLink() : LocalTwinCATLink() {}
  ~LocalTwinCATLink() override = default;
  LocalTwinCATLink(const LocalTwinCATLink& v) noexcept = delete;
  LocalTwinCATLink& operator=(const LocalTwinCATLink& obj) = delete;
  LocalTwinCATLink(LocalTwinCATLink&& obj) = delete;
  LocalTwinCATLink& operator=(LocalTwinCATLink&& obj) = delete;

 protected:
  Result<bool, std::string> Open() override;
  Result<bool, std::string> Close() override;
  Result<bool, std::string> Send(size_t size, const uint8_t* buf) override;
  Result<bool, std::string> Read(uint8_t* rx, uint32_t buffer_len) override;
  bool is_open() override;

 private:
  std::string _ams_net_id;
  std::string _ipv4_addr;
  long _port = 0L;  // NOLINT
  AmsNetId _net_id;
#ifdef _WIN32
  HMODULE _lib = nullptr;
#endif
};

LinkPtr LocalTwinCATLink::Create() {
  LinkPtr link = std::make_unique<LocalTwinCATLinkImpl>();
  return link;
}

bool LocalTwinCATLinkImpl::is_open() { return this->_port > 0; }

#ifdef _WIN32
typedef long(_stdcall* TcAdsPortOpenEx)();                       // NOLINT
typedef long(_stdcall* TcAdsPortCloseEx)(long);                  // NOLINT
typedef long(_stdcall* TcAdsGetLocalAddressEx)(long, AmsAddr*);  // NOLINT
typedef long(_stdcall* TcAdsSyncWriteReqEx)(long, AmsAddr*,      // NOLINT
                                            unsigned long,       // NOLINT
                                            unsigned long,       // NOLINT
                                            unsigned long,       // NOLINT
                                            void*);              // NOLINT
typedef long(_stdcall* TcAdsSyncReadReqEx)(long, AmsAddr*,       // NOLINT
                                           unsigned long,        // NOLINT
                                           unsigned long,        // NOLINT
                                           unsigned long,        // NOLINT
                                           void*,                // NOLINT
                                           unsigned long*);      // NOLINT

constexpr auto TCADS_ADS_PORT_OPEN_EX = "AdsPortOpenEx";
constexpr auto TCADS_ADS_GET_LOCAL_ADDRESS_EX = "AdsGetLocalAddressEx";
constexpr auto TCADS_ADS_PORT_CLOSE_EX = "AdsPortCloseEx";
constexpr auto TCADS_ADS_SYNC_WRITE_REQ_EX = "AdsSyncWriteReqEx";
constexpr auto TCADS_ADS_SYNC_READ_REQ_EX = "AdsSyncReadReqEx2";

Result<bool, std::string> LocalTwinCATLinkImpl::Open() {
  this->_lib = LoadLibrary("TcAdsDll.dll");
  if (_lib == nullptr) return Err(std::string("couldn't find TcADS-DLL"));

  const auto port_open = reinterpret_cast<TcAdsPortOpenEx>(GetProcAddress(this->_lib, TCADS_ADS_PORT_OPEN_EX));
  this->_port = (*port_open)();
  if (!this->_port) return Err(std::string("Error: Failed to open a new ADS port"));

  AmsAddr addr;
  const auto get_addr = reinterpret_cast<TcAdsGetLocalAddressEx>(GetProcAddress(this->_lib, TCADS_ADS_GET_LOCAL_ADDRESS_EX));
  const auto ret = get_addr(this->_port, &addr);  // NOLINT
  if (ret) {
    std::stringstream ss;
    ss << "Error: AdsGetLocalAddress: " << std::hex << ret;
    return Err(ss.str());
  }

  this->_net_id = addr.netId;
  return Ok(true);
}
Result<bool, std::string> LocalTwinCATLinkImpl::Close() {
  this->_port = 0;
  const auto port_close = reinterpret_cast<TcAdsPortCloseEx>(GetProcAddress(this->_lib, TCADS_ADS_PORT_CLOSE_EX));
  const auto res = (*port_close)(this->_port);
  if (res == 0) return Ok(true);
  std::stringstream ss;
  ss << "Error on closing (local): " << std::hex << res;
  return Err(ss.str());
}

Result<bool, std::string> LocalTwinCATLinkImpl::Send(const size_t size, const uint8_t* buf) {
  AmsAddr addr = {this->_net_id, PORT};
  const auto write = reinterpret_cast<TcAdsSyncWriteReqEx>(GetProcAddress(this->_lib, TCADS_ADS_SYNC_WRITE_REQ_EX));
  const auto ret = write(this->_port,  // NOLINT
                         &addr, INDEX_GROUP, INDEX_OFFSET_BASE,
                         static_cast<unsigned long>(size),  // NOLINT
                         const_cast<void*>(static_cast<const void*>(buf)));

  if (ret == 0) return Ok(true);
  // https://infosys.beckhoff.com/english.php?content=../content/1033/tcadscommon/html/tcadscommon_intro.htm&id=
  // 6 : target port not found
  std::stringstream ss;
  ss << "Error on sending data (local): " << std::hex << ret;
  return Err(ss.str());
}

Result<bool, std::string> LocalTwinCATLinkImpl::Read(uint8_t* rx, const uint32_t buffer_len) {
  AmsAddr addr = {this->_net_id, PORT};
  const auto read = reinterpret_cast<TcAdsSyncReadReqEx>(GetProcAddress(this->_lib, TCADS_ADS_SYNC_READ_REQ_EX));

  unsigned long read_bytes;           // NOLINT
  const auto ret = read(this->_port,  // NOLINT
                        &addr, INDEX_GROUP, INDEX_OFFSET_BASE_READ, buffer_len, rx, &read_bytes);
  if (ret == 0) return Ok(true);

  std::stringstream ss;
  ss << "Error on reading data: " << std::hex << ret;
  return Err(ss.str());
}

#else
Result<bool, std::string> LocalTwinCATLinkImpl::Open() {
  return Err(std::string("Link to localhost has not been compiled. Rebuild this library on a Twincat3 host machine with TcADS-DLL."));
}
Result<bool, std::string> LocalTwinCATLinkImpl::Close() { return Ok(false); }
Result<bool, std::string> LocalTwinCATLinkImpl::Send(size_t size, const uint8_t* buf) {
  (void)size;
  (void)buf;
  return Ok(false);
}
Result<bool, std::string> LocalTwinCATLinkImpl::Read(uint8_t* rx, uint32_t buffer_len) {
  (void)rx;
  (void)buffer_len;
  return Ok(false);
}
#endif  // TC_ADS

}  // namespace autd::link
