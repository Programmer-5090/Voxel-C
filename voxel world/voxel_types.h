#ifndef VOXEL_TYPES_H
#define VOXEL_TYPES_H

#include <cstdint>

// Basic voxel type definitions
using VoxelID = uint16_t;
using ChunkCoord = int32_t;

// Voxel constants
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_HEIGHT = 64;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;
constexpr int WATER_LEVEL = 55;

// Voxel types
enum VoxelType : VoxelID
{
    VOXEL_AIR = 0,
    VOXEL_STONE = 1,
    VOXEL_DIRT = 2,
    VOXEL_GRASS = 3,
    VOXEL_COBBLESTONE = 4,
    VOXEL_WOOD = 5,
    VOXEL_LEAVES = 6,
    VOXEL_SAND = 7,
    VOXEL_WATER = 8,
    VOXEL_GLASS = 9,
    VOXEL_IRON = 10,
    VOXEL_COUNT = 11
};

// Voxel properties
struct VoxelInfo
{
    const char *name;
    bool is_solid;
    bool is_transparent;
    float texture_top;    // Top face texture index
    float texture_bottom; // Bottom face texture index
    float texture_sides;  // Side faces texture index
};

// Voxel database - properties for each voxel type
static const VoxelInfo VOXEL_INFO[VOXEL_COUNT] = {
    //   Name Solid  Transp  Top    Bottom Sides
    {"Air", false, true, 0.0f, 0.0f, 0.0f},         // VOXEL_AIR
    {"Stone", true, false, 1.0f, 1.0f, 1.0f},       // VOXEL_STONE (stone.png)
    {"Dirt", true, false, 2.0f, 2.0f, 2.0f},        // VOXEL_DIRT (dirt.png)
    {"Grass", true, false, 3.0f, 2.0f, 4.0f},       // VOXEL_GRASS (grass_top, dirt, grass_side)
    {"Cobblestone", true, false, 5.0f, 5.0f, 5.0f}, // VOXEL_COBBLESTONE
    {"Wood", true, false, 6.0f, 6.0f, 7.0f},        // VOXEL_WOOD (oak_log_top, oak_log_top, oak_log)
    {"Leaves", true, true, 8.0f, 8.0f, 8.0f},       // VOXEL_LEAVES
    {"Sand", true, false, 9.0f, 9.0f, 9.0f},        // VOXEL_SAND
    {"Water", false, true, 10.0f, 10.0f, 10.0f},    // VOXEL_WATER (animated, base frame)
    {"Glass", true, true, 42.0f, 42.0f, 42.0f},     // VOXEL_GLASS (moved after water frames)
    {"Iron", true, false, 43.0f, 43.0f, 43.0f}      // VOXEL_IRON (moved after water frames)
};

// Helper functions
inline bool isVoxelSolid(VoxelID voxel)
{
    return voxel < VOXEL_COUNT && VOXEL_INFO[voxel].is_solid;
}

inline bool isVoxelTransparent(VoxelID voxel)
{
    return voxel >= VOXEL_COUNT || VOXEL_INFO[voxel].is_transparent;
}

inline const char *getVoxelName(VoxelID voxel)
{
    return voxel < VOXEL_COUNT ? VOXEL_INFO[voxel].name : "Unknown";
}

#endif // VOXEL_TYPES_H
