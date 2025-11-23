#include "NELogger.h"
#include "SpawnDataHelper.h"

#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "GlobalNamespace/BeatmapObjectData.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"

#include "custom-json-data/shared/CustomBeatmapData.h"

#include "tracks/shared/Vector.h"
#include "NECaches.h"

using namespace GlobalNamespace;

float SpawnDataHelper::HighestJumpPosYForLineLayer(float lineLayer)
{
    // Magic numbers below found with linear regression y=mx+b using existing HighestJumpPosYForLineLayer values

    return (0.875f * lineLayer) + 0.639583f + NECaches::JumpOffsetYProvider->jumpOffsetY;
}

float SpawnDataHelper::GetGravityBase(float noteLineLayer, float beforeJumpLineLayer)
{
  return HighestJumpPosYForLineLayer(noteLineLayer) - LineYPosForLineLayer(beforeJumpLineLayer);
}

float SpawnDataHelper::LineYPosForLineLayer(float height) {
  return 0.25f + (height * NECaches::get_noteLinesDistanceFast()); // offset by 0.25
}
