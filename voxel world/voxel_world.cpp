#include "voxel_world.h"
#include <algorithm>
#include <iostream>
#include <cmath>

VoxelWorld::VoxelWorld(uint32_t seed, int render_distance)
    : world_seed(seed), render_distance(render_distance), last_center_chunk(INT_MAX)
{
}

VoxelWorld::~VoxelWorld()
{
    // Unique pointers will automatically clean up
}

void VoxelWorld::update(const glm::vec3 &center_position)
{
    updateChunksAroundPosition(center_position);
    processChunkLoadingQueue();
    processChunkUnloadingQueue();
}

void VoxelWorld::updateChunksAroundPosition(const glm::vec3 &position)
{
    glm::ivec3 center_chunk = worldToChunk(position);

    // Only update if the center chunk has changed
    if (center_chunk == last_center_chunk)
    {
        return;
    }

    last_center_chunk = center_chunk;

    // Get chunks that should be loaded (now sorted by distance)
    std::vector<glm::ivec3> desired_chunks = getChunksInRange(center_chunk, render_distance);

    // Find chunks to load (they're already in priority order)
    chunks_to_load.clear();
    for (const auto &chunk_pos : desired_chunks)
    {
        if (!isChunkLoaded(chunk_pos))
        {
            chunks_to_load.push_back(chunk_pos);
        }
    }

    // Find chunks to unload (chunks that are too far away)
    chunks_to_unload.clear();
    for (const auto &[chunk_pos, chunk] : chunks)
    {
        glm::ivec3 diff = chunk_pos - center_chunk;
        // Use proper distance calculation for unloading too
        float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y * 0.25f + diff.z * diff.z);

        if (distance > render_distance + 1.5f)
        { // +1.5 for hysteresis to prevent thrashing
            chunks_to_unload.push_back(chunk_pos);
        }
    }
}

VoxelID VoxelWorld::getVoxel(int x, int y, int z) const
{
    return getVoxel(glm::ivec3(x, y, z));
}

VoxelID VoxelWorld::getVoxel(const glm::ivec3 &pos) const
{
    glm::ivec3 chunk_pos = worldToChunk(pos);
    const VoxelChunk *chunk = getChunk(chunk_pos);

    if (!chunk)
    {
        return VOXEL_AIR;
    }

    glm::ivec3 local_pos = worldToLocal(pos);
    return chunk->getVoxel(local_pos);
}

void VoxelWorld::setVoxel(int x, int y, int z, VoxelID voxel)
{
    setVoxel(glm::ivec3(x, y, z), voxel);
}

void VoxelWorld::setVoxel(const glm::ivec3 &pos, VoxelID voxel)
{
    glm::ivec3 chunk_pos = worldToChunk(pos);
    VoxelChunk *chunk = getOrCreateChunk(chunk_pos);

    if (chunk)
    {
        glm::ivec3 local_pos = worldToLocal(pos);
        chunk->setVoxel(local_pos, voxel);
    }
}

VoxelChunk *VoxelWorld::getChunk(const glm::ivec3 &chunk_pos)
{
    auto it = chunks.find(chunk_pos);
    return (it != chunks.end()) ? it->second.get() : nullptr;
}

const VoxelChunk *VoxelWorld::getChunk(const glm::ivec3 &chunk_pos) const
{
    auto it = chunks.find(chunk_pos);
    return (it != chunks.end()) ? it->second.get() : nullptr;
}

VoxelChunk *VoxelWorld::getOrCreateChunk(const glm::ivec3 &chunk_pos)
{
    auto it = chunks.find(chunk_pos);
    if (it != chunks.end())
    {
        return it->second.get();
    }

    // Create new chunk
    auto chunk = std::make_unique<VoxelChunk>(chunk_pos);
    VoxelChunk *chunk_ptr = chunk.get();
    chunks[chunk_pos] = std::move(chunk);

    // Generate the chunk
    chunk_ptr->generate(world_seed);

    // Update neighbors
    updateChunkNeighbors(chunk_pos);

    return chunk_ptr;
}

void VoxelWorld::loadChunk(const glm::ivec3 &chunk_pos)
{
    if (isChunkLoaded(chunk_pos))
    {
        return;
    }

    getOrCreateChunk(chunk_pos);
}

void VoxelWorld::unloadChunk(const glm::ivec3 &chunk_pos)
{
    auto it = chunks.find(chunk_pos);
    if (it != chunks.end())
    {
        // Update neighbors to remove references to this chunk
        VoxelChunk *chunk = it->second.get();
        for (int i = 0; i < 6; i++)
        {
            VoxelChunk *neighbor = chunk->getNeighbor(i);
            if (neighbor)
            {
                // Find the opposite direction
                int opposite_dir = (i % 2 == 0) ? i + 1 : i - 1;
                neighbor->setNeighbor(opposite_dir, nullptr);
            }
        }

        chunks.erase(it);
    }
}

bool VoxelWorld::isChunkLoaded(const glm::ivec3 &chunk_pos) const
{
    return chunks.find(chunk_pos) != chunks.end();
}

glm::ivec3 VoxelWorld::worldToChunk(const glm::ivec3 &world_pos)
{
    return glm::ivec3(
        world_pos.x >= 0 ? world_pos.x / CHUNK_SIZE : (world_pos.x + 1) / CHUNK_SIZE - 1,
        world_pos.y >= 0 ? world_pos.y / CHUNK_HEIGHT : (world_pos.y + 1) / CHUNK_HEIGHT - 1,
        world_pos.z >= 0 ? world_pos.z / CHUNK_SIZE : (world_pos.z + 1) / CHUNK_SIZE - 1);
}

glm::ivec3 VoxelWorld::worldToChunk(const glm::vec3 &world_pos)
{
    return worldToChunk(glm::ivec3(std::floor(world_pos.x), std::floor(world_pos.y), std::floor(world_pos.z)));
}

glm::ivec3 VoxelWorld::worldToLocal(const glm::ivec3 &world_pos)
{
    glm::ivec3 chunk_pos = worldToChunk(world_pos);
    glm::ivec3 chunk_world_pos = chunkToWorld(chunk_pos);
    return world_pos - chunk_world_pos;
}

glm::ivec3 VoxelWorld::chunkToWorld(const glm::ivec3 &chunk_pos)
{
    return glm::ivec3(
        chunk_pos.x * CHUNK_SIZE,
        chunk_pos.y * CHUNK_HEIGHT,
        chunk_pos.z * CHUNK_SIZE);
}

void VoxelWorld::updateChunkNeighbors(const glm::ivec3 &chunk_pos)
{
    VoxelChunk *chunk = getChunk(chunk_pos);
    if (!chunk)
    {
        return;
    }

    linkChunkNeighbors(chunk);
}

void VoxelWorld::updateAllNeighbors()
{
    for (auto &[pos, chunk] : chunks)
    {
        linkChunkNeighbors(chunk.get());
    }
}

void VoxelWorld::setRenderDistance(int distance)
{
    render_distance = std::max(1, distance);
    last_center_chunk = glm::ivec3(INT_MAX); // Force update
}

void VoxelWorld::processChunkLoadingQueue()
{
    // Load a few chunks per frame to avoid frame drops
    int chunks_to_load_per_frame = 2;
    int loaded_count = 0;

    auto it = chunks_to_load.begin();
    while (it != chunks_to_load.end() && loaded_count < chunks_to_load_per_frame)
    {
        loadChunk(*it);
        it = chunks_to_load.erase(it);
        loaded_count++;
    }
}

void VoxelWorld::processChunkUnloadingQueue()
{
    // Unload chunks immediately
    for (const auto &chunk_pos : chunks_to_unload)
    {
        unloadChunk(chunk_pos);
    }
    chunks_to_unload.clear();
}

std::vector<glm::ivec3> VoxelWorld::getChunksInRange(const glm::ivec3 &center, int range) const
{
    // Structure to hold distance and chunk position with proper comparison
    struct ChunkDistance
    {
        float distance;
        glm::ivec3 position;

        // Comparison operator for sorting (nearest first)
        bool operator<(const ChunkDistance &other) const
        {
            return distance < other.distance;
        }
    };

    std::vector<ChunkDistance> chunks_with_distance;

    for (int x = center.x - range; x <= center.x + range; x++)
    {
        for (int y = std::max(0, center.y - 2); y <= std::min(7, center.y + 2); y++)
        { // Limit Y range
            for (int z = center.z - range; z <= center.z + range; z++)
            {
                glm::ivec3 chunk_pos(x, y, z);
                glm::ivec3 diff = chunk_pos - center;

                // Use proper distance calculation (Euclidean distance)
                float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y * 0.25f + diff.z * diff.z);

                // Only include chunks within the circular range
                if (distance <= range)
                {
                    chunks_with_distance.push_back({distance, chunk_pos});
                }
            }
        }
    }

    // Sort chunks by distance (nearest first)
    std::sort(chunks_with_distance.begin(), chunks_with_distance.end());

    // Extract just the chunk positions in distance order
    std::vector<glm::ivec3> result;
    result.reserve(chunks_with_distance.size());
    for (const auto &chunk_data : chunks_with_distance)
    {
        result.push_back(chunk_data.position);
    }

    return result;
}

void VoxelWorld::linkChunkNeighbors(VoxelChunk *chunk)
{
    if (!chunk)
    {
        return;
    }

    glm::ivec3 pos = chunk->position;

    // Define neighbor offsets
    static const glm::ivec3 neighbor_offsets[6] = {
        glm::ivec3(0, 0, 1),  // NEIGHBOR_FRONT
        glm::ivec3(0, 0, -1), // NEIGHBOR_BACK
        glm::ivec3(1, 0, 0),  // NEIGHBOR_RIGHT
        glm::ivec3(-1, 0, 0), // NEIGHBOR_LEFT
        glm::ivec3(0, 1, 0),  // NEIGHBOR_TOP
        glm::ivec3(0, -1, 0)  // NEIGHBOR_BOTTOM
    };

    for (int i = 0; i < 6; i++)
    {
        glm::ivec3 neighbor_pos = pos + neighbor_offsets[i];
        VoxelChunk *neighbor = getChunk(neighbor_pos);

        chunk->setNeighbor(i, neighbor);

        // Set reciprocal neighbor relationship
        if (neighbor)
        {
            int opposite_dir = (i % 2 == 0) ? i + 1 : i - 1;
            neighbor->setNeighbor(opposite_dir, chunk);
        }
    }
}
