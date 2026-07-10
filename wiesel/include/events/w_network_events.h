
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

#include "events/w_events.h"
#include "w_pch.h"

namespace wiesel {

class NetworkClientConnectedEvent : public Event {
 public:
  explicit NetworkClientConnectedEvent(uint64_t session_id)
      : session_id_(session_id) {}

  WIESEL_GETTER_FN uint64_t session_id() const { return session_id_; }

  EVENT_CLASS_TYPE(NetworkClientConnected)
  EVENT_CLASS_CATEGORY(kEventCategoryNetwork)
 private:
  uint64_t session_id_;
};

class NetworkClientDisconnectedEvent : public Event {
 public:
  explicit NetworkClientDisconnectedEvent(uint64_t session_id)
      : session_id_(session_id) {}

  WIESEL_GETTER_FN uint64_t session_id() const { return session_id_; }

  EVENT_CLASS_TYPE(NetworkClientDisconnected)
  EVENT_CLASS_CATEGORY(kEventCategoryNetwork)
 private:
  uint64_t session_id_;
};

class NetworkConnectedToServerEvent : public Event {
 public:
  NetworkConnectedToServerEvent() = default;

  EVENT_CLASS_TYPE(NetworkConnectedToServer)
  EVENT_CLASS_CATEGORY(kEventCategoryNetwork)
};

class NetworkDisconnectedFromServerEvent : public Event {
 public:
  NetworkDisconnectedFromServerEvent() = default;

  EVENT_CLASS_TYPE(NetworkDisconnectedFromServer)
  EVENT_CLASS_CATEGORY(kEventCategoryNetwork)
};

class NetworkServerStartedEvent : public Event {
 public:
  NetworkServerStartedEvent() = default;

  EVENT_CLASS_TYPE(NetworkServerStarted)
  EVENT_CLASS_CATEGORY(kEventCategoryNetwork)
};

class NetworkServerStoppedEvent : public Event {
 public:
  NetworkServerStoppedEvent() = default;

  EVENT_CLASS_TYPE(NetworkServerStopped)
  EVENT_CLASS_CATEGORY(kEventCategoryNetwork)
};

}  // namespace wiesel
