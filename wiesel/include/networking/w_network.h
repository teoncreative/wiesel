
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.h"

namespace znet {
class Buffer;
class Codec;
class Packet;
class PeerSession;
class PacketSerializerBase;
using PacketId = uint64_t;
}  // namespace znet

namespace wiesel {

enum class NetworkRole {
  kNone = 0,
  kServer = 1,
  kClient = 2,
  kListenServer = 3,
};

struct NetworkServerConfig {
  std::string bind_ip = "0.0.0.0";
  uint16_t port = 25000;
  int timeout_seconds = 10;
};

struct NetworkClientConfig {
  std::string server_ip = "127.0.0.1";
  uint16_t port = 25000;
  int timeout_seconds = 10;
};

using SessionSetupCallback =
    std::function<void(std::shared_ptr<znet::PeerSession>)>;

class NetworkManager {
 public:
  NetworkManager();
  ~NetworkManager();

  bool Init();
  void Shutdown();

  bool StartServer(const NetworkServerConfig& config);
  void StopServer();

  bool ConnectToServer(const NetworkClientConfig& config);
  void Disconnect();

  void RegisterPacket(znet::PacketId id,
                      std::unique_ptr<znet::PacketSerializerBase> serializer);

  void SetSessionSetupCallback(SessionSetupCallback callback);

  WIESEL_GETTER_FN std::shared_ptr<znet::PeerSession> GetSession(
      uint64_t session_id) const;
  WIESEL_GETTER_FN std::shared_ptr<znet::PeerSession> GetServerSession() const;

  void Broadcast(std::shared_ptr<znet::Packet> packet);
  void SendTo(uint64_t session_id, std::shared_ptr<znet::Packet> packet);
  void ForEachSession(
      std::function<void(uint64_t, std::shared_ptr<znet::PeerSession>)> fn)
      const;

  // RPCs (entity-scoped)
  void SendServerRpc(uint32_t net_id, const std::string& rpc_name,
                     std::shared_ptr<znet::Buffer> args = nullptr);
  void SendClientRpc(uint32_t net_id, const std::string& rpc_name,
                     std::shared_ptr<znet::Buffer> args = nullptr);

  void SetTickRate(int ticks_per_second);
  WIESEL_GETTER_FN int tick_rate() const;
  WIESEL_GETTER_FN float tick_interval() const;

  // C++ callback registration
  using NetworkEventCallback = std::function<void(uint64_t)>;
  using NetworkSimpleCallback = std::function<void()>;
  void OnClientConnected(NetworkEventCallback callback);
  void OnClientDisconnected(NetworkEventCallback callback);
  void OnConnectedToServer(NetworkSimpleCallback callback);
  void OnDisconnectedFromServer(NetworkSimpleCallback callback);

  WIESEL_GETTER_FN bool is_server() const;
  WIESEL_GETTER_FN bool is_client() const;
  WIESEL_GETTER_FN bool is_connected() const;
  WIESEL_GETTER_FN NetworkRole role() const;
  WIESEL_GETTER_FN uint64_t local_session_id() const;

  void Update();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace wiesel
