
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

#include "asset/w_asset_handle.h"
#include "w_pch.h"

namespace Wiesel {

// Interface for type-specific asset loaders.
// Each asset type (Model, Texture, Audio, etc.) registers a loader.
class IAssetLoader {
 public:
  virtual ~IAssetLoader() = default;

  // Load the asset synchronously. Called from either the main thread
  // (sync load) or a background thread (async load).
  // Returns true on success.
  virtual bool Load(AssetHandle handle) = 0;

  // Unload the asset, freeing GPU/memory resources.
  virtual void Unload(AssetHandle handle) = 0;
};

// Convenience: wraps a load function into an IAssetLoader.
class FunctionAssetLoader : public IAssetLoader {
 public:
  using LoadFn = std::function<bool(AssetHandle)>;
  using UnloadFn = std::function<void(AssetHandle)>;

  FunctionAssetLoader(LoadFn load_fn, UnloadFn unload_fn = nullptr)
      : load_fn_(std::move(load_fn)), unload_fn_(std::move(unload_fn)) {}

  bool Load(AssetHandle handle) override { return load_fn_(handle); }

  void Unload(AssetHandle handle) override {
    if (unload_fn_) {
      unload_fn_(handle);
    }
  }

 private:
  LoadFn load_fn_;
  UnloadFn unload_fn_;
};

}  // namespace Wiesel