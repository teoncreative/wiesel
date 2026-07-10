
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

#include "networking/w_replication_types.h"
#include "scene/w_entity.h"
#include "w_pch.h"

namespace znet {
class Buffer;
}

namespace wiesel {

struct NetworkComponentSerializerDesc {
  NetworkComponentTypeId type_id;
  std::string debug_name;
  std::function<bool(Entity&)> Has;
  std::function<void(Entity&, znet::Buffer&)> Serialize;
  std::function<void(Entity&, znet::Buffer&)> Deserialize;
  std::function<bool(Entity&)> HasChanged;
};

class NetworkComponentSerializerRegistry {
 public:
  static NetworkComponentTypeId Register(NetworkComponentSerializerDesc desc);

  static const std::vector<NetworkComponentSerializerDesc>& Registry();

  static void SerializeDirty(Entity& entity, NetworkIdentityComponent& net_id,
                             znet::Buffer& buffer);

  static void SerializeAll(Entity& entity, znet::Buffer& buffer);

  static void DeserializeAll(Entity& entity, znet::Buffer& buffer);

  static void UpdateDirtyFlags(Entity& entity,
                               NetworkIdentityComponent& net_id);
};

void InitializeNetworkComponentSerializers();

}  // namespace wiesel
