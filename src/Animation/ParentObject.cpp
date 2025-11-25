#include "Animation/ParentObject.h"

#include "AssociatedData.h"
#include "NELogger.h"
#include "beatsaber-hook/shared/utils/il2cpp-type-check.hpp"

#include <utility>
#include "Animation/AnimationHelper.h"
#include "UnityEngine/GameObject.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/StaticBeatmapObjectSpawnMovementData.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "NECaches.h"

using namespace TrackParenting;
using namespace UnityEngine;
using namespace GlobalNamespace;
using namespace Animation;

template <>
struct std::hash<std::pair<TrackW, ParentObject*>> {
  std::size_t operator()(const std::pair<TrackW, ParentObject*>& k) const {
    return std::hash<int>()(k.first.track._0) ^ std::hash<ParentObject*>()(k.second);
  }
};
// emulate delegates here by associating each Track + Parent to a callback
// and since callbacks are one time use, we cannot recycle themm when removing them
// those will cause a crash since they're freed when RemoveGameObjectCallback is called
static std::unordered_map<std::pair<TrackW, ParentObject*>, TrackW::CWrappedCallback> gameObjectModificationCallbacks;

static void RemoveCallback(TrackW track, ParentObject* object) {
  auto pair = std::pair(track, object);
  track.RemoveGameObjectCallback(gameObjectModificationCallbacks[pair]);
  gameObjectModificationCallbacks.erase(pair);
}

template<typename F>
static void AddCallback(TrackW track, ParentObject* object, F&& f) {
  auto pair = std::pair(track, object);
  auto callback = track.RegisterGameObjectCallback(std::forward<F>(f));
  gameObjectModificationCallbacks[pair] = callback;
}

// Events.cpp
extern BeatmapObjectSpawnController* spawnController;

DEFINE_TYPE(TrackParenting, ParentObject);

void ParentObject::OnEnable() {
  OnTransformParentChanged();
}

void ParentObject::Update() {
  UpdateData(false);
}

void ParentObject::OnTransformParentChanged() {
  UpdateData(true);
}

void ParentObject::UpdateData(bool force) {
  if (!track) return;

  if (track.v2) {
    UpdateDataOld(force);
    return;
  }
  if (force) {
    lastCheckedTime = TimeUnit();
  }

  auto const rotation = track.GetPropertyNamed(PropertyNames::Rotation).GetQuat(lastCheckedTime);
  auto const localRotation = track.GetPropertyNamed(PropertyNames::LocalRotation).GetQuat(lastCheckedTime);
  auto const position = track.GetPropertyNamed(PropertyNames::Position).GetVec3(lastCheckedTime);
  auto const localPosition = track.GetPropertyNamed(PropertyNames::LocalPosition).GetVec3(lastCheckedTime);
  auto const scale = track.GetPropertyNamed(PropertyNames::Scale).GetVec3(lastCheckedTime);

  auto transform = origin;

  if (rotation) {
    transform->set_rotation(rotation.value());
  } else if (localRotation) {
    transform->set_localRotation(localRotation.value());
  }

  if (position) {
    transform->set_position(position.value());
  } else if (localPosition) {
    transform->set_localPosition(localPosition.value());
  }

  if (scale) {
    transform->set_localScale(scale.value());
  }

  lastCheckedTime = getCurrentTime();
}

void ParentObject::UpdateDataOld(bool forced) {
  if (forced) {
    lastCheckedTime = TimeUnit();
  }
  float noteLinesDistance = GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;

  auto const rotation = track.GetPropertyNamed(PropertyNames::Rotation).GetQuat(lastCheckedTime);
  auto const localRotation = track.GetPropertyNamed(PropertyNames::LocalRotation).GetQuat(lastCheckedTime);
  auto const position = track.GetPropertyNamed(PropertyNames::Position).GetVec3(lastCheckedTime);
  auto const scale = track.GetPropertyNamed(PropertyNames::Scale).GetVec3(lastCheckedTime);

  NEVector::Quaternion worldRotationQuaternion = startRot;
  NEVector::Vector3 positionVector = worldRotationQuaternion * (startPos * noteLinesDistance);
  if (rotation.has_value() || position.has_value()) {
    NEVector::Quaternion rotationOffset = rotation.value_or(NEVector::Quaternion::identity());
    worldRotationQuaternion = worldRotationQuaternion * rotationOffset;
    NEVector::Vector3 positionOffset = position.value_or(NEVector::Vector3::zero());
    positionVector = worldRotationQuaternion * ((positionOffset + startPos) * noteLinesDistance);
  }

  worldRotationQuaternion = worldRotationQuaternion * startLocalRot;

  if (localRotation.has_value()) {
    worldRotationQuaternion = worldRotationQuaternion * *localRotation;
  }

  Vector3 scaleVector = startScale;
  if (scale.has_value()) {
    scaleVector = startScale * scale.value();
  }

  origin->set_localRotation(worldRotationQuaternion);
  origin->set_localPosition(positionVector);
  origin->set_localScale(scaleVector);

  lastCheckedTime = getCurrentTime();
}

static void logTransform(Transform* transform, int hierarchy = 0) {
  if (hierarchy != 0) {
    std::string tab = std::string(hierarchy * 4, ' ');
    NELogger::Logger.debug("{}{}Child: {} {}", hierarchy, tab.c_str(), transform->get_gameObject()->get_name(),
                           transform->GetChildCount());
  } else {
    NELogger::Logger.debug("Self: {} {}", transform->get_gameObject()->get_name(), transform->GetChildCount());
  }
  for (int i = 0; i < transform->GetChildCount(); i++) {
    auto childTransform = transform->GetChild(i);
    logTransform(childTransform, hierarchy + 1);
  }
}

void ParentObject::AssignTrack(ParentTrackEventData const& parentTrackEventData) {
  static ConstString ParentName("ParentObject");

  GameObject* parentGameObject = GameObject::New_ctor(ParentName);
  ParentObject* instance = parentGameObject->AddComponent<ParentObject*>();

  static auto get_transform =
      il2cpp_utils::il2cpp_type_check::FPtrWrapper<&UnityEngine::GameObject::get_transform>::get();

  instance->origin = get_transform(parentGameObject);
  instance->track = parentTrackEventData.parentTrack;
  instance->worldPositionStays = parentTrackEventData.worldPositionStays;

  Transform* transform = instance->origin;
  NELogger::Logger.debug("Assigning ParentObject {} to [{}] v2 {}", parentTrackEventData.parentTrack.GetName(),
                         fmt::join(parentTrackEventData.childrenTracks |
                                       std::views::transform([](auto& t) { return std::string(t.GetName()); }),
                                   ", "),
                         parentTrackEventData.parentTrack.v2);

  auto startTime = std::chrono::high_resolution_clock::now();
  parentTrackEventData.parentTrack.RegisterGameObject(parentGameObject);

  for (auto& track : parentTrackEventData.childrenTracks) {
    if (track == parentTrackEventData.parentTrack) {
      NELogger::Logger.error("How could a track contain itself?");
    }


    for (auto parentObject : ParentController::parentObjects) {
      // track->gameObjectModificationEvent -= { &ParentObject::HandleGameObject, parentObject };
      // parentObject->childrenTracks.erase(track);

      // this code is ugly but whatever, keep the original above as a reference
      if (!parentObject) continue;

      RemoveCallback(track, parentObject);


      parentObject->childrenTracks.erase(track);
    }

    // NELogger::Logger.debug("Reparenting {} from {} to {}", childTrack.GetGameObjects().size(),
    //                        childTrack.GetName(), parentTrackEventData.parentTrack.GetName());
    for (auto& gameObject : track.GetGameObjects()) {
      instance->ParentToObject(get_transform(gameObject));
    }
    // instance->childrenTracks.emplace(track);
    // track.gameObjectModificationEvent += { &ParentObject::HandleGameObject, instance };

    instance->childrenTracks.emplace(track);
    
    RemoveCallback(track, instance);
    AddCallback(track, instance, [instance](UnityEngine::GameObject* go, bool added) {
      instance->HandleGameObject(instance->track, go, !added);
    });
  }

  ParentController::parentObjects.emplace_back(instance);

  if (instance->track.v2) {
    if (parentTrackEventData.pos.has_value()) {
      instance->startPos = *parentTrackEventData.pos;
      transform->set_localPosition(instance->startPos * StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance);
    }

    if (parentTrackEventData.rot.has_value()) {
      instance->startRot = *parentTrackEventData.rot;
      instance->startLocalRot = instance->startRot;
      transform->set_localPosition(instance->startRot * NEVector::Vector3(transform->get_localPosition()));
      transform->set_localRotation(instance->startRot);
    }

    if (parentTrackEventData.localRot.has_value()) {
      instance->startLocalRot = instance->startRot * *parentTrackEventData.localRot;
      transform->set_localRotation(NEVector::Quaternion(transform->get_localRotation()) * instance->startLocalRot);
    }

    if (parentTrackEventData.scale.has_value()) {
      instance->startScale = *parentTrackEventData.scale;
      transform->set_localScale(instance->startScale);
    }
  } else {
    if (parentTrackEventData.pos.has_value()) {
      transform->set_position(instance->startPos);
    } else if (parentTrackEventData.localPos.has_value()) {
      transform->set_localPosition(instance->startPos);
    }

    if (parentTrackEventData.rot.has_value()) {
      transform->set_localRotation(instance->startRot);
    } else if (parentTrackEventData.localRot.has_value()) {
      transform->set_localRotation(instance->startLocalRot);
    }

    if (parentTrackEventData.scale.has_value()) {
      transform->set_localScale(instance->startScale);
    }
  }
}

void ParentObject::ParentToObject(Transform* transform) {
  static auto SetParent =
      il2cpp_utils::il2cpp_type_check::FPtrWrapper<static_cast<void (Transform::*)(UnityEngine::Transform*, bool)>(
          &UnityEngine::Transform::SetParent)>::get();

  SetParent(transform, origin, worldPositionStays);
}

void ParentObject::ResetTransformParent(Transform* transform) {
  static auto SetParent =
      il2cpp_utils::il2cpp_type_check::FPtrWrapper<static_cast<void (Transform::*)(UnityEngine::Transform*, bool)>(
          &UnityEngine::Transform::SetParent)>::get();

  SetParent(transform, nullptr, false);
}

void ParentObject::HandleGameObject(TrackW track, UnityEngine::GameObject* go, bool removed) {
  static auto get_transform =
      il2cpp_utils::il2cpp_type_check::FPtrWrapper<&UnityEngine::GameObject::get_transform>::get();

  if (removed) {
    ResetTransformParent(get_transform(go));
  } else {
    ParentToObject(get_transform(go));
  }
}

ParentObject::~ParentObject() {
  // for (auto& childTrack : childrenTracks) {
  // childTrack->gameObjectModificationEvent -= { &ParentObject::HandleGameObject, this };
  //}
  // just in case
  // track->gameObjectModificationEvent -= { &ParentObject::HandleGameObject, this };

  // ParentController::parentObjects.erase(
  //     std::remove(ParentController::parentObjects.begin(), ParentController::parentObjects.end(), this),
  //     ParentController::parentObjects.end());
}

void ParentController::OnDestroy() {
  NELogger::Logger.debug("Clearing parent objects");
  parentObjects.clear();
  gameObjectModificationCallbacks.clear();
}