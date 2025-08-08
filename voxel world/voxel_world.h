#ifndef VOXEL_WORLD_H
#define VOXEL_WORLD_H

#include "voxel_types.h"
#include "voxel_chunk.h"
#include <glm/glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>

// Forward declarations
class Camera;

// Hash function for glm::ivec3 to use as key in unordered_map
struct Vec3Hash
{
    std::size_t operator()(const glm::ivec3 &v) const
    {
        std::size_t h1 = std::hash<int>{}(v.x);
        std::size_t h2 = std::hash<int>{}(v.y);
        std::size_t h3 = std::hash<int>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class VoxelWorld
{
public:
    using ChunkMap = std::unordered_map<glm::ivec3, std::unique_ptr<VoxelChunk>, Vec3Hash>;

private:
    ChunkMap chunks;
    uint32_t world_seed;
    int render_distance;
    glm::ivec3 last_center_chunk;

    // Chunk loading/unloading queues
    std::vector<glm::ivec3> chunks_to_load;
    std::vector<glm::ivec3> chunks_to_unload;

public:
    explicit VoxelWorld(uint32_t seed, int render_distance = 8);
    ~VoxelWorld();

    // World management
    void update(const glm::vec3 &center_position);
    void updateChunksAroundPosition(const glm::vec3 &position);

    // Voxel access
    VoxelID getVoxel(int x, int y, int z) const;
    VoxelID getVoxel(const glm::ivec3 &pos) const;
    void setVoxel(int x, int y, int z, VoxelID voxel);
    void setVoxel(const glm::ivec3 &pos, VoxelID voxel);

    // Chunk access
    VoxelChunk *getChunk(const glm::ivec3 &chunk_pos);
    const VoxelChunk *getChunk(const glm::ivec3 &chunk_pos) const;
    VoxelChunk *getOrCreateChunk(const glm::ivec3 &chunk_pos);

    // Chunk management
    void loadChunk(const glm::ivec3 &chunk_pos);
    void unloadChunk(const glm::ivec3 &chunk_pos);
    bool isChunkLoaded(const glm::ivec3 &chunk_pos) const;

    // Coordinate conversion
    static glm::ivec3 worldToChunk(const glm::ivec3 &world_pos);
    static glm::ivec3 worldToChunk(const glm::vec3 &world_pos);
    static glm::ivec3 worldToLocal(const glm::ivec3 &world_pos);
    static glm::ivec3 chunkToWorld(const glm::ivec3 &chunk_pos);

    // Neighbor management
    void updateChunkNeighbors(const glm::ivec3 &chunk_pos);
    void updateAllNeighbors();

    // Getters
    const ChunkMap &getChunks() const { return chunks; }
    int getRenderDistance() const { return render_distance; }
    uint32_t getSeed() const { return world_seed; }
    size_t getLoadedChunkCount() const { return chunks.size(); }

    // Settings
    void setRenderDistance(int distance);

private:
    // Internal helper functions
    void processChunkLoadingQueue();
    void processChunkUnloadingQueue();
    std::vector<glm::ivec3> getChunksInRange(const glm::ivec3 &center, int range) const;
    void linkChunkNeighbors(VoxelChunk *chunk);
};

#endif // VOXEL_WORLD_H
