
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "networking/w_network.h"

#include "events/w_network_events.h"
#include "networking/w_replication_packets.h"
#include "networking/w_replication_types.h"
#include "w_engine.h"
#include "znet/client.h"
#include "znet/client_events.h"
#include "znet/codec.h"
#include "znet/init.h"
#include "znet/packet.h"
#include "znet/peer_session.h"
#include "znet/server.h"
#include "znet/server_events.h"

namespace wiesel {

struct NetworkManager::Impl {
  bool initialized = false;
  NetworkRole role = NetworkRole::kNone;

  std::shared_ptr<znet::Codec> codec;
  std::unique_ptr<znet::Server> server;
  std::unique_ptr<znet::Client> client;
  std::shared_ptr<znet::PeerSession> client_session;

  SessionSetupCallback session_setup_callback;

  std::mutex sessions_mutex;
  std::unordered_map<uint64_t, std::shared_ptr<znet::PeerSession>> sessions;

  int tick_rate = 20;
  float tick_interval = 1.0f / 20.0f;


  // C++ callbacks
  std::vector<NetworkManager::NetworkEventCallback> on_client_connected;
  std::vector<NetworkManager::NetworkEventCallback> on_client_disconnected;
  std::vector<NetworkManager::NetworkSimpleCallback> on_connected_to_server;
  std::vector<NetworkManager::NetworkSimpleCallback> on_disconnected_from_server;

  std::mutex event_queue_mutex;
  std::vector<std::function<void()>> event_queue;

  void EnqueueEvent(std::function<void()> fn) {
    std::scoped_lock lock(event_queue_mutex);
    event_queue.emplace_back(std::move(fn));
  }

  void AddSession(std::shared_ptr<znet::PeerSession> session) {
    std::scoped_lock lock(sessions_mutex);
    sessions[session->id()] = session;
  }

  void RemoveSession(uint64_t session_id) {
    std::scoped_lock lock(sessions_mutex);
    sessions.erase(session_id);
  }
};

NetworkManager::NetworkManager() : impl_(std::make_unique<Impl>()) {}

NetworkManager::~NetworkManager() {
  Shutdown();
}

bool NetworkManager::Init() {
  if (impl_->initialized) {
    return true;
  }

  znet::Result result = znet::Init();
  if (result != znet::Result::Success) {
    LOG_ERROR("Failed to initialize znet: {}",
              znet::GetResultString(result));
    return false;
  }

  impl_->codec = std::make_shared<znet::Codec>();
  impl_->initialized = true;
  LOG_INFO("Network manager initialized");
  return true;
}

void NetworkManager::Shutdown() {
  if (!impl_ || !impl_->initialized) {
    return;
  }

  StopServer();
  Disconnect();

  impl_->codec = nullptr;
  znet::Cleanup();
  impl_->initialized = false;
  LOG_INFO("Network manager shut down");
}

bool NetworkManager::StartServer(const NetworkServerConfig& config) {
  if (!impl_->initialized) {
    LOG_ERROR("Cannot start server: NetworkManager not initialized");
    return false;
  }

  if (impl_->server) {
    LOG_WARN("Server already running, stopping first");
    StopServer();
  }

  znet::ServerConfig znet_config{
      config.bind_ip,
      static_cast<znet::PortNumber>(config.port),
      std::chrono::seconds(config.timeout_seconds),
  };

  impl_->server = std::make_unique<znet::Server>(znet_config);

  impl_->server->SetEventCallback([this](znet::Event& event) {
    znet::EventDispatcher dispatcher(event);

    dispatcher.Dispatch<znet::IncomingClientConnectedEvent>(
        [this](znet::IncomingClientConnectedEvent& e) {
          auto session = e.session();
          session->SetCodec(impl_->codec);

          // Add to map BEFORE callback so SyncToSession/SendTo can find it
          impl_->AddSession(session);

          if (impl_->session_setup_callback) {
            impl_->session_setup_callback(session);
          }

          uint64_t session_id = session->id();
          impl_->EnqueueEvent([this, session_id]() {
            for (auto& cb : impl_->on_client_connected) {
              cb(session_id);
            }
            NetworkClientConnectedEvent event(session_id);
            Engine::BroadcastEvent(event);
          });
          return false;
        });

    dispatcher.Dispatch<znet::ServerClientDisconnectedEvent>(
        [this](znet::ServerClientDisconnectedEvent& e) {
          uint64_t session_id = e.session()->id();
          impl_->RemoveSession(session_id);

          impl_->EnqueueEvent([this, session_id]() {
            for (auto& cb : impl_->on_client_disconnected) {
              cb(session_id);
            }
            NetworkClientDisconnectedEvent event(session_id);
            Engine::BroadcastEvent(event);
          });
          return false;
        });

    dispatcher.Dispatch<znet::ServerStartupEvent>(
        [this](znet::ServerStartupEvent& e) {
          impl_->EnqueueEvent([]() {
            NetworkServerStartedEvent event;
            Engine::BroadcastEvent(event);
          });
          return false;
        });

    dispatcher.Dispatch<znet::ServerShutdownEvent>(
        [this](znet::ServerShutdownEvent& e) {
          impl_->EnqueueEvent([]() {
            NetworkServerStoppedEvent event;
            Engine::BroadcastEvent(event);
          });
          return false;
        });
  });

  znet::Result result = impl_->server->Bind();
  if (result != znet::Result::Success) {
    LOG_ERROR("Failed to bind server: {}", znet::GetResultString(result));
    impl_->server = nullptr;
    return false;
  }

  result = impl_->server->Listen();
  if (result != znet::Result::Success) {
    LOG_ERROR("Failed to start server: {}", znet::GetResultString(result));
    impl_->server = nullptr;
    return false;
  }

  if (impl_->role == NetworkRole::kClient) {
    impl_->role = NetworkRole::kListenServer;
  } else {
    impl_->role = NetworkRole::kServer;
  }

  LOG_INFO("Server started on {}:{}", config.bind_ip, config.port);
  return true;
}

void NetworkManager::StopServer() {
  if (!impl_->server) {
    return;
  }

  impl_->server->Stop();
  impl_->server->Wait();
  impl_->server = nullptr;

  {
    std::scoped_lock lock(impl_->sessions_mutex);
    impl_->sessions.clear();
  }

  if (impl_->role == NetworkRole::kListenServer) {
    impl_->role = NetworkRole::kClient;
  } else {
    impl_->role = NetworkRole::kNone;
  }

  LOG_INFO("Server stopped");
}

bool NetworkManager::ConnectToServer(const NetworkClientConfig& config) {
  if (!impl_->initialized) {
    LOG_ERROR("Cannot connect: NetworkManager not initialized");
    return false;
  }

  if (impl_->client) {
    LOG_WARN("Already connected, disconnecting first");
    Disconnect();
  }

  znet::ClientConfig znet_config{
      config.server_ip,
      static_cast<znet::PortNumber>(config.port),
      std::chrono::seconds(config.timeout_seconds),
  };

  impl_->client = std::make_unique<znet::Client>(znet_config);

  impl_->client->SetEventCallback([this](znet::Event& event) {
    znet::EventDispatcher dispatcher(event);

    dispatcher.Dispatch<znet::ClientConnectedToServerEvent>(
        [this](znet::ClientConnectedToServerEvent& e) {
          auto session = e.session();
          session->SetCodec(impl_->codec);
          impl_->client_session = session;

          if (impl_->session_setup_callback) {
            impl_->session_setup_callback(session);
          }

          impl_->EnqueueEvent([this]() {
            for (auto& cb : impl_->on_connected_to_server) {
              cb();
            }
            NetworkConnectedToServerEvent event;
            Engine::BroadcastEvent(event);
          });
          return false;
        });

    dispatcher.Dispatch<znet::ClientDisconnectedFromServerEvent>(
        [this](znet::ClientDisconnectedFromServerEvent& e) {
          impl_->client_session = nullptr;

          impl_->EnqueueEvent([this]() {
            for (auto& cb : impl_->on_disconnected_from_server) {
              cb();
            }
            NetworkDisconnectedFromServerEvent event;
            Engine::BroadcastEvent(event);
          });
          return false;
        });
  });

  znet::Result result = impl_->client->Bind();
  if (result != znet::Result::Success) {
    LOG_ERROR("Failed to bind client: {}", znet::GetResultString(result));
    impl_->client = nullptr;
    return false;
  }

  result = impl_->client->Connect();
  if (result != znet::Result::Success) {
    LOG_ERROR("Failed to connect to server: {}",
              znet::GetResultString(result));
    impl_->client = nullptr;
    return false;
  }

  if (impl_->role == NetworkRole::kServer) {
    impl_->role = NetworkRole::kListenServer;
  } else {
    impl_->role = NetworkRole::kClient;
  }

  LOG_INFO("Connecting to {}:{}", config.server_ip, config.port);
  return true;
}

void NetworkManager::Disconnect() {
  if (!impl_->client) {
    return;
  }

  impl_->client->Disconnect();
  impl_->client->Wait();
  impl_->client = nullptr;
  impl_->client_session = nullptr;

  if (impl_->role == NetworkRole::kListenServer) {
    impl_->role = NetworkRole::kServer;
  } else {
    impl_->role = NetworkRole::kNone;
  }

  LOG_INFO("Disconnected from server");
}

void NetworkManager::RegisterPacket(
    znet::PacketId id,
    std::unique_ptr<znet::PacketSerializerBase> serializer) {
  if (!impl_ || !impl_->initialized) {
    LOG_ERROR("Cannot register packet: NetworkManager not initialized");
    return;
  }
  impl_->codec->Add(id, std::move(serializer));
}

void NetworkManager::SetSessionSetupCallback(SessionSetupCallback callback) {
  impl_->session_setup_callback = std::move(callback);
}

std::shared_ptr<znet::PeerSession> NetworkManager::GetSession(
    uint64_t session_id) const {
  std::scoped_lock lock(impl_->sessions_mutex);
  auto it = impl_->sessions.find(session_id);
  if (it != impl_->sessions.end()) {
    return it->second;
  }
  return nullptr;
}

std::shared_ptr<znet::PeerSession> NetworkManager::GetServerSession() const {
  return impl_->client_session;
}

void NetworkManager::Broadcast(std::shared_ptr<znet::Packet> packet) {
  std::scoped_lock lock(impl_->sessions_mutex);
  for (auto& [id, session] : impl_->sessions) {
    session->SendPacket(packet);
  }
}

void NetworkManager::SendTo(uint64_t session_id,
                            std::shared_ptr<znet::Packet> packet) {
  std::scoped_lock lock(impl_->sessions_mutex);
  auto it = impl_->sessions.find(session_id);
  if (it != impl_->sessions.end()) {
    it->second->SendPacket(packet);
  }
}

void NetworkManager::ForEachSession(
    std::function<void(uint64_t, std::shared_ptr<znet::PeerSession>)> fn)
    const {
  std::scoped_lock lock(impl_->sessions_mutex);
  for (auto& [id, session] : impl_->sessions) {
    fn(id, session);
  }
}

void NetworkManager::SendServerRpc(uint32_t net_id,
                                   const std::string& rpc_name,
                                   std::shared_ptr<znet::Buffer> args) {
  auto packet = std::make_shared<RpcPacket>();
  packet->net_id = net_id;
  packet->direction = RpcDirection::kToServer;
  packet->rpc_name = rpc_name;
  packet->args_data = std::move(args);

  auto session = GetServerSession();
  if (session) {
    session->SendPacket(packet);
  }
}

void NetworkManager::SendClientRpc(uint32_t net_id,
                                   const std::string& rpc_name,
                                   std::shared_ptr<znet::Buffer> args) {
  auto packet = std::make_shared<RpcPacket>();
  packet->net_id = net_id;
  packet->direction = RpcDirection::kToClients;
  packet->rpc_name = rpc_name;
  packet->args_data = std::move(args);

  Broadcast(packet);
}

void NetworkManager::OnClientConnected(NetworkEventCallback callback) {
  impl_->on_client_connected.push_back(std::move(callback));
}

void NetworkManager::OnClientDisconnected(NetworkEventCallback callback) {
  impl_->on_client_disconnected.push_back(std::move(callback));
}

void NetworkManager::OnConnectedToServer(NetworkSimpleCallback callback) {
  impl_->on_connected_to_server.push_back(std::move(callback));
}

void NetworkManager::OnDisconnectedFromServer(NetworkSimpleCallback callback) {
  impl_->on_disconnected_from_server.push_back(std::move(callback));
}

void NetworkManager::SetTickRate(int ticks_per_second) {
  if (ticks_per_second < 1) {
    ticks_per_second = 1;
  }
  impl_->tick_rate = ticks_per_second;
  impl_->tick_interval = 1.0f / static_cast<float>(ticks_per_second);
}

int NetworkManager::tick_rate() const {
  return impl_->tick_rate;
}

float NetworkManager::tick_interval() const {
  return impl_->tick_interval;
}

bool NetworkManager::is_server() const {
  return impl_->role == NetworkRole::kServer ||
         impl_->role == NetworkRole::kListenServer;
}

bool NetworkManager::is_client() const {
  return impl_->role == NetworkRole::kClient ||
         impl_->role == NetworkRole::kListenServer;
}

bool NetworkManager::is_connected() const {
  return impl_->client_session != nullptr;
}

NetworkRole NetworkManager::role() const {
  return impl_->role;
}

void NetworkManager::Update() {
  if (!impl_ || !impl_->initialized) {
    return;
  }

  std::vector<std::function<void()>> pending;
  {
    std::scoped_lock lock(impl_->event_queue_mutex);
    pending.swap(impl_->event_queue);
  }
  for (auto& fn : pending) {
    fn();
  }
}

}  // namespace wiesel
