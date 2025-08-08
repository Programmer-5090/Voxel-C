#ifndef VOXEL_CHUNK_H
#define VOXEL_CHUNK_H

#include "voxel_types.h"
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <memory>

// Forward declarations
class VoxelWorld;
class ChunkMesh;
class VoxelNoise; // forward declare to avoid heavy include in header

class VoxelChunk
{
public:
    static constexpr int SIZE = CHUNK_SIZE;
    static constexpr int HEIGHT = CHUNK_HEIGHT;
    static constexpr int VOLUME = CHUNK_VOLUME;

    // Chunk position in world chunk coordinates
    glm::ivec3 position;

    // Version number for tracking changes
    uint64_t version;

    // World seed used for generation
    uint32_t generation_seed;

    // Chunk state flags
    bool is_generated;
    bool is_dirty;
    bool is_mesh_dirty;
    bool is_meshing;

    // Voxel data storage
    std::array<VoxelID, VOLUME> voxels;

    // Neighboring chunks (for mesh generation)
    std::array<VoxelChunk *, 6> neighbors;

    // Mesh data
    std::unique_ptr<ChunkMesh> mesh;

private:
    // Cached terrain column heights (world ground height for each local x,z)
    std::array<int, SIZE * SIZE> column_heights{}; // filled during generate()
    bool has_column_cache = false;
    // Cached noise generator (lazily re-created if needed)
    std::unique_ptr<VoxelNoise> noise_generator;

    // Extended noise cache for neighboring block lookups
    // Covers area from (-1,-1) to (SIZE,SIZE) in local coordinates
    std::array<int, (SIZE + 2) * (SIZE + 2)> extended_terrain_heights;
    bool has_extended_noise_cache;

    inline int columnIndex(int x, int z) const { return x * SIZE + z; }

public:
    VoxelChunk(const glm::ivec3 &pos);
    ~VoxelChunk();

    // Voxel access
    VoxelID getVoxel(int x, int y, int z) const;
    VoxelID getVoxel(const glm::ivec3 &pos) const;
    void setVoxel(int x, int y, int z, VoxelID voxel);
    void setVoxel(const glm::ivec3 &pos, VoxelID voxel);

    // Safe voxel access (checks bounds)
    VoxelID getVoxelSafe(int x, int y, int z) const;
    VoxelID getVoxelSafe(const glm::ivec3 &pos) const;

    // Neighbor management
    void setNeighbor(int direction, VoxelChunk *neighbor);
    VoxelChunk *getNeighbor(int direction) const;

    // Generation and mesh building
    void generate(uint32_t seed);
    void buildMesh();
    bool needsMeshRebuild() const;

    // Utility functions
    bool isInBounds(int x, int y, int z) const;
    bool isInBounds(const glm::ivec3 &pos) const;
    glm::ivec3 worldToLocal(const glm::ivec3 &worldPos) const;
    glm::ivec3 localToWorld(const glm::ivec3 &localPos) const;

    // Get voxel from neighboring chunk if needed
    VoxelID getVoxelWithNeighbors(int x, int y, int z) const;

    // Generate expected voxel at position using terrain generation logic
    VoxelID generateExpectedVoxel(int x, int y, int z) const;

    // Meshing status
    bool isMeshing() const { return is_meshing; }
    void setMeshing(bool status) { is_meshing = status; }

private:
    // Convert 3D coordinates to 1D array index
    inline int coordsToIndex(int x, int y, int z) const
    {
        return x * HEIGHT * SIZE + y * SIZE + z;
    }

    // Convert 1D array index to 3D coordinates
    inline glm::ivec3 indexToCoords(int index) const
    {
        int x = index / (HEIGHT * SIZE);
        int remainder = index % (HEIGHT * SIZE);
        int y = remainder / SIZE;
        int z = remainder % SIZE;
        return glm::ivec3(x, y, z);
    }

    // Helper functions
    void calculateExtendedNoiseCache();
    int getTerrainHeightFromCache(int x, int z) const;
    int calculateTerrainHeightAt(int x, int z) const;
    VoxelID generateExpectedVoxelFromCache(int x, int y, int z) const;
};

// Neighbor directions
enum NeighborDirection
{
    NEIGHBOR_FRONT = 0, // +Z
    NEIGHBOR_BACK = 1,  // -Z
    NEIGHBOR_RIGHT = 2, // +X
    NEIGHBOR_LEFT = 3,  // -X
    NEIGHBOR_TOP = 4,   // +Y
    NEIGHBOR_BOTTOM = 5 // -Y
};

// Face directions for mesh generation
enum FaceDirection
{
    FACE_FRONT = 0, // +Z
    FACE_BACK = 1,  // -Z
    FACE_RIGHT = 2, // +X
    FACE_LEFT = 3,  // -X
    FACE_TOP = 4,   // +Y
    FACE_BOTTOM = 5 // -Y
};

// Face direction vectors
static const glm::ivec3 FACE_NORMALS[6] = {
    glm::ivec3(0, 0, 1),  // FACE_FRONT
    glm::ivec3(0, 0, -1), // FACE_BACK
    glm::ivec3(1, 0, 0),  // FACE_RIGHT
    glm::ivec3(-1, 0, 0), // FACE_LEFT
    glm::ivec3(0, 1, 0),  // FACE_TOP
    glm::ivec3(0, -1, 0)  // FACE_BOTTOM
};

#endif // VOXEL_CHUNK_H
