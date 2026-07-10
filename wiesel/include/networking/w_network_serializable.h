
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
}

namespace wiesel {

// C++ interface for network-serializable types.
// Implement this on custom structs to use them in RPCs and sync vars.
class INetworkSerializable {
 public:
  virtual ~INetworkSerializable() = default;
  virtual void NetworkSerialize(znet::Buffer& buffer) const = 0;
  virtual void NetworkDeserialize(znet::Buffer& buffer) = 0;
};

// Supported primitive types for network serialization
enum class NetworkSerializableType : uint8_t {
  kInt = 0,
  kFloat = 1,
  kBool = 2,
  kString = 3,
  kVec3 = 4,
  kCustom = 255,
};

// Describes a single field for auto-serialization (used by C# reflection)
struct NetworkFieldDesc {
  std::string name;
  NetworkSerializableType type;
  uint32_t offset;  // byte offset within the struct (for Mono field access)
};

// Describes a type's serialization plan (built from reflection)
struct NetworkTypeSerializationPlan {
  std::string type_name;
  std::vector<NetworkFieldDesc> fields;
};

// Describes a discovered RPC method
struct RpcMethodDesc {
  std::string rpc_name;  // without "ServerRpc_" / "ClientRpc_" prefix
  bool is_server_rpc;
  void* mono_method;  // MonoMethod*
  std::vector<NetworkSerializableType> param_types;
  std::vector<NetworkTypeSerializationPlan> param_plans;  // for custom types
};

}  // namespace wiesel
