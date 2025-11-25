// https://github.com/Kautenja/object-pool/blob/master/include/object_pool.hpp

#pragma once

#include <list>
#include "Animation/NoodleMovementDataProvider.hpp"
#include "GlobalNamespace/BeatmapObjectData.hpp"
#include "NELogger.h"

namespace NoodleExtensions::Pool {

class NoodleMovementDataProviderPool {
private:
  std::deque<SafePtr<NoodleMovementDataProvider>> free = {};

public:
  NoodleMovementDataProviderPool(int count) : free() {
    for (int i = 0; i < count; ++i) {
      put(NoodleMovementDataProvider::New_ctor());
    }
  }

  SafePtr<NoodleMovementDataProvider> get(GlobalNamespace::BeatmapObjectData* beatmapObjectData) {
    SafePtr<NoodleMovementDataProvider> obj;
    if (!free.empty()) {
      obj.emplace(free.back().ptr());
      free.pop_back();
    }

    if (!obj) {
      obj.emplace(NoodleMovementDataProvider::New_ctor());
    }
    obj->InitObject(beatmapObjectData);
    return obj;
  }

  void put(SafePtr<NoodleMovementDataProvider> obj) {
    // reset variables here if needed
    // obj->InitObject(nullptr);
    free.emplace_back(obj);
  }
};

} // namespace NoodleExtensions::Pool
