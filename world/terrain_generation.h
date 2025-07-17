#pragma once

#include <array>
#include "coordinate.h"
#include "world_constants.h"
#include <vector>
#include <string>

class ChunkManager;
class VoxelDataManager;

std::vector<ChunkPosition> generateTerrain(ChunkManager& chunkManager, int chunkX,
                                           int chunkZ, const VoxelDataManager& voxelData,
                                           int seed, int worldSize);

float generateSeed(const std::string& input);
