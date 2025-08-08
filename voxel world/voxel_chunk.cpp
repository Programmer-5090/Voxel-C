#include "voxel_chunk.h"
#include "voxel_world.h"
#include "voxel_noise.h"
#include "chunk_mesh.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <string>

VoxelChunk::VoxelChunk(const glm::ivec3 &pos)
    : position(pos), version(0), generation_seed(0), is_generated(false), is_dirty(false), is_mesh_dirty(false), is_meshing(false)
{
    voxels.fill(VOXEL_AIR);
    neighbors.fill(nullptr);
    mesh = std::make_unique<ChunkMesh>();
    has_column_cache = false;
    has_extended_noise_cache = false;
}

VoxelChunk::~VoxelChunk()
{
    // ChunkMesh destructor will handle cleanup
}

VoxelID VoxelChunk::getVoxel(int x, int y, int z) const
{
    if (!isInBounds(x, y, z))
    {
        return VOXEL_AIR;
    }
    return voxels[coordsToIndex(x, y, z)];
}

VoxelID VoxelChunk::getVoxel(const glm::ivec3 &pos) const
{
    return getVoxel(pos.x, pos.y, pos.z);
}

void VoxelChunk::setVoxel(int x, int y, int z, VoxelID voxel)
{
    if (!isInBounds(x, y, z))
    {
        return;
    }

    int index = coordsToIndex(x, y, z);
    if (voxels[index] != voxel)
    {
        voxels[index] = voxel;
        version++;
        is_dirty = true;
        is_mesh_dirty = true;

        // Mark neighboring chunks as dirty if we're on a boundary
        if (x == 0 && neighbors[NEIGHBOR_LEFT])
        {
            neighbors[NEIGHBOR_LEFT]->is_mesh_dirty = true;
        }
        if (x == SIZE - 1 && neighbors[NEIGHBOR_RIGHT])
        {
            neighbors[NEIGHBOR_RIGHT]->is_mesh_dirty = true;
        }
        if (y == 0 && neighbors[NEIGHBOR_BOTTOM])
        {
            neighbors[NEIGHBOR_BOTTOM]->is_mesh_dirty = true;
        }
        if (y == HEIGHT - 1 && neighbors[NEIGHBOR_TOP])
        {
            neighbors[NEIGHBOR_TOP]->is_mesh_dirty = true;
        }
        if (z == 0 && neighbors[NEIGHBOR_BACK])
        {
            neighbors[NEIGHBOR_BACK]->is_mesh_dirty = true;
        }
        if (z == SIZE - 1 && neighbors[NEIGHBOR_FRONT])
        {
            neighbors[NEIGHBOR_FRONT]->is_mesh_dirty = true;
        }
    }
}

void VoxelChunk::setVoxel(const glm::ivec3 &pos, VoxelID voxel)
{
    setVoxel(pos.x, pos.y, pos.z, voxel);
}

VoxelID VoxelChunk::getVoxelSafe(int x, int y, int z) const
{
    if (isInBounds(x, y, z))
    {
        return voxels[coordsToIndex(x, y, z)];
    }

    // Try to get from neighboring chunks
    return getVoxelWithNeighbors(x, y, z);
}

VoxelID VoxelChunk::getVoxelSafe(const glm::ivec3 &pos) const
{
    return getVoxelSafe(pos.x, pos.y, pos.z);
}

void VoxelChunk::setNeighbor(int direction, VoxelChunk *neighbor)
{
    if (direction >= 0 && direction < 6)
    {
        neighbors[direction] = neighbor;
    }
}

VoxelChunk *VoxelChunk::getNeighbor(int direction) const
{
    if (direction >= 0 && direction < 6)
    {
        return neighbors[direction];
    }
    return nullptr;
}

void VoxelChunk::generate(uint32_t seed)
{
    if (is_generated)
    {
        return;
    }

    auto generation_start = std::chrono::high_resolution_clock::now();

    generation_seed = seed;

    // Create / reset noise generator once
    noise_generator = std::make_unique<VoxelNoise>(seed);

    auto noise_cache_start = std::chrono::high_resolution_clock::now();
    // Pre-calculate noise for extended area (chunk + 1 block border on all sides)
    calculateExtendedNoiseCache();
    auto noise_cache_end = std::chrono::high_resolution_clock::now();

    VoxelNoise &noise = *noise_generator;
    glm::ivec3 worldPos = position * glm::ivec3(SIZE, HEIGHT, SIZE);

    auto voxel_generation_start = std::chrono::high_resolution_clock::now();
    int voxels_processed = 0;
    for (int x = 0; x < SIZE; x++)
    {
        for (int z = 0; z < SIZE; z++)
        {
            // Use cached terrain height instead of recalculating
            int terrainHeight = getTerrainHeightFromCache(x, z);
            column_heights[columnIndex(x, z)] = terrainHeight;

            for (int y = 0; y < HEIGHT; y++)
            {
                int worldY = worldPos.y + y;
                VoxelID voxel = VOXEL_AIR;
                if (worldY < terrainHeight - 3)
                    voxel = VOXEL_STONE;
                else if (worldY < terrainHeight - 1)
                    voxel = VOXEL_DIRT;
                else if (worldY < terrainHeight)
                    voxel = VOXEL_GRASS;
                else if (worldY <= WATER_LEVEL && worldY >= terrainHeight)
                    voxel = VOXEL_WATER;
                setVoxel(x, y, z, voxel);
                voxels_processed++;
            }
        }
    }
    auto voxel_generation_end = std::chrono::high_resolution_clock::now();

    has_column_cache = true;
    is_generated = true;
    is_dirty = false;
    is_mesh_dirty = true;
    is_meshing = false;
    version++;

    auto generation_end = std::chrono::high_resolution_clock::now();

    // Calculate and log generation timing
    float noise_cache_time = std::chrono::duration<float, std::milli>(noise_cache_end - noise_cache_start).count();
    float voxel_gen_time = std::chrono::duration<float, std::milli>(voxel_generation_end - voxel_generation_start).count();
    float total_generation_time = std::chrono::duration<float, std::milli>(generation_end - generation_start).count();

    if (total_generation_time > 5.0f) // Only log if generation takes more than 5ms
    {
        std::string log_message =
            "CHUNK GENERATION TIMING for chunk (" + std::to_string(position.x) + ", " +
            std::to_string(position.y) + ", " + std::to_string(position.z) + "):\n" +
            "  Noise Cache: " + std::to_string(noise_cache_time) + "ms\n" +
            "  Voxel Generation: " + std::to_string(voxel_gen_time) + "ms (" +
            std::to_string(voxels_processed) + " voxels)\n" +
            "  TOTAL GENERATION: " + std::to_string(total_generation_time) + "ms\n" +
            "  Cache entries: " + std::to_string((SIZE + 2) * (SIZE + 2)) + "\n";

        std::cout << log_message << std::endl;
    }
}

void VoxelChunk::calculateExtendedNoiseCache()
{
    if (!noise_generator)
    {
        return;
    }

    VoxelNoise &noise = *noise_generator;
    glm::ivec3 worldPos = position * glm::ivec3(SIZE, HEIGHT, SIZE);

    // Static splines
    static const std::vector<SplinePoint> continentalSpline = {
        {-1.0f, 30.0f}, {-0.5f, 50.0f}, {0.0f, 80.0f}, {0.3f, 100.0f}, {0.6f, 130.0f}, {1.0f, 160.0f}};
    static const std::vector<SplinePoint> erosionSpline = {
        {-1.0f, 0.0f}, {0.0f, 10.0f}, {0.5f, 25.0f}, {1.0f, 40.0f}};

    float noiseScale = 0.005f;

    // Pre-allocate arrays for batch noise processing if available
    const int cacheSize = (SIZE + 2) * (SIZE + 2);

    auto noise_calculation_start = std::chrono::high_resolution_clock::now();
    int noise_calculations = 0;

    // Calculate for minimal extended area: -1 to SIZE (inclusive) in both X and Z
    // Only cache the 1-block border we actually need for meshing
    for (int x = -1; x <= SIZE; x++)
    {
        for (int z = -1; z <= SIZE; z++)
        {
            int worldX = worldPos.x + x;
            int worldZ = worldPos.z + z;

            auto clamp = [](float v) -> float
            { return std::max(-1.0f, std::min(1.0f, v)); };

            // Use faster noise lookups with reduced precision where possible
            float noiseX = worldX * noiseScale;
            float noiseZ = worldZ * noiseScale;

            float continentalness = clamp(noise.getContinentalness(noiseX, noiseZ));
            float erosion = clamp(noise.getErosion(noiseX, noiseZ));

            float baseHeight = noise.evalSpline(continentalSpline, continentalness);
            float erosionEffect = noise.evalSpline(erosionSpline, erosion);
            float terrainHeightF = (baseHeight - erosionEffect);

            // Only calculate expensive peaks/valleys for areas that need it
            if (erosion < 0.3f)
            {
                float peaksAndValleys = clamp(noise.getPeaksandValleysGenerator(noiseX, noiseZ));
                float mountainFactor = std::max(0.0f, peaksAndValleys - erosion);
                // Use faster approximation instead of pow(x, 1.5)
                mountainFactor = mountainFactor * mountainFactor * sqrtf(mountainFactor);
                terrainHeightF += mountainFactor * 50.0f;
            }

            int terrainHeight = (int)terrainHeightF;

            // Store in extended cache (convert local coords to cache index)
            int cacheX = x + 1; // -1 becomes 0, 0 becomes 1, etc.
            int cacheZ = z + 1;
            extended_terrain_heights[cacheX * (SIZE + 2) + cacheZ] = terrainHeight;
            noise_calculations++;
        }
    }
    auto noise_calculation_end = std::chrono::high_resolution_clock::now();

    has_extended_noise_cache = true;

    // Log detailed noise timing if it's slow
    float noise_calc_time = std::chrono::duration<float, std::milli>(noise_calculation_end - noise_calculation_start).count();
    if (noise_calc_time > 8.0f)
    {
        std::cout << "SLOW NOISE CACHE for chunk (" << position.x << ", " << position.y << ", " << position.z
                  << "): " << noise_calc_time << "ms for " << noise_calculations << " calculations" << std::endl;
    }
}

int VoxelChunk::getTerrainHeightFromCache(int x, int z) const
{
    if (!has_extended_noise_cache)
    {
        // Fallback to live calculation
        return calculateTerrainHeightAt(x, z);
    }

    // Convert to cache coordinates
    int cacheX = x + 1;
    int cacheZ = z + 1;

    if (cacheX >= 0 && cacheX < (SIZE + 2) && cacheZ >= 0 && cacheZ < (SIZE + 2))
    {
        return extended_terrain_heights[cacheX * (SIZE + 2) + cacheZ];
    }

    // Outside cache bounds - fallback
    return calculateTerrainHeightAt(x, z);
}

int VoxelChunk::calculateTerrainHeightAt(int x, int z) const
{
    if (!noise_generator)
    {
        return 64; // Default height
    }

    VoxelNoise &noise = *noise_generator;
    glm::ivec3 chunkBase = position * glm::ivec3(SIZE, HEIGHT, SIZE);
    int worldX = chunkBase.x + x;
    int worldZ = chunkBase.z + z;

    static const std::vector<SplinePoint> continentalSpline = {
        {-1.0f, 30.0f}, {-0.5f, 50.0f}, {0.0f, 80.0f}, {0.3f, 100.0f}, {0.6f, 130.0f}, {1.0f, 160.0f}};
    static const std::vector<SplinePoint> erosionSpline = {
        {-1.0f, 0.0f}, {0.0f, 10.0f}, {0.5f, 25.0f}, {1.0f, 40.0f}};

    float noiseScale = 0.005f;
    auto clamp = [](float v) -> float
    { return std::max(-1.0f, std::min(1.0f, v)); };

    // Use faster noise lookups
    float noiseX = worldX * noiseScale;
    float noiseZ = worldZ * noiseScale;

    float continentalness = clamp(noise.getContinentalness(noiseX, noiseZ));
    float erosion = clamp(noise.getErosion(noiseX, noiseZ));

    float baseHeight = noise.evalSpline(continentalSpline, continentalness);
    float erosionEffect = noise.evalSpline(erosionSpline, erosion);
    float terrainHeightF = (baseHeight - erosionEffect);

    // Only calculate expensive peaks/valleys for areas that need it
    if (erosion < 0.3f)
    {
        float peaksAndValleys = clamp(noise.getPeaksandValleysGenerator(noiseX, noiseZ));
        float mountainFactor = std::max(0.0f, peaksAndValleys - erosion);
        // Use faster approximation: x^1.5 ≈ x * x * sqrt(x)
        mountainFactor = mountainFactor * mountainFactor * sqrtf(mountainFactor);
        terrainHeightF += mountainFactor * 50.0f;
    }

    return (int)terrainHeightF;
}

VoxelID VoxelChunk::getVoxelWithNeighbors(int x, int y, int z) const
{
    if (isInBounds(x, y, z))
    {
        return voxels[coordsToIndex(x, y, z)];
    }

    // Fast reject if more than 1 block out
    if (x < -1 || x > SIZE || y < -1 || y > HEIGHT || z < -1 || z > SIZE)
    {
        return VOXEL_STONE;
    }

    VoxelChunk *neighbor = nullptr;
    glm::ivec3 neighborPos(x, y, z);

    if (x == -1)
    {
        neighbor = neighbors[NEIGHBOR_LEFT];
        neighborPos.x = SIZE - 1;
    }
    else if (x == SIZE)
    {
        neighbor = neighbors[NEIGHBOR_RIGHT];
        neighborPos.x = 0;
    }
    else if (y == -1)
    {
        neighbor = neighbors[NEIGHBOR_BOTTOM];
        neighborPos.y = HEIGHT - 1;
    }
    else if (y == HEIGHT)
    {
        neighbor = neighbors[NEIGHBOR_TOP];
        neighborPos.y = 0;
    }
    else if (z == -1)
    {
        neighbor = neighbors[NEIGHBOR_BACK];
        neighborPos.z = SIZE - 1;
    }
    else if (z == SIZE)
    {
        neighbor = neighbors[NEIGHBOR_FRONT];
        neighborPos.z = 0;
    }

    if (neighbor && neighbor->isInBounds(neighborPos))
    {
        return neighbor->getVoxel(neighborPos);
    }

    // Use cached noise instead of returning placeholder
    return generateExpectedVoxelFromCache(x, y, z);
}

VoxelID VoxelChunk::generateExpectedVoxelFromCache(int x, int y, int z) const
{
    if (isInBounds(x, y, z))
    {
        return voxels[coordsToIndex(x, y, z)];
    }

    // Get terrain height from cache (works for x,z in range [-1, SIZE])
    int terrainHeight = getTerrainHeightFromCache(x, z);

    glm::ivec3 chunkBase = position * glm::ivec3(SIZE, HEIGHT, SIZE);
    int worldY = chunkBase.y + y;

    if (worldY < terrainHeight - 3)
        return VOXEL_STONE;
    if (worldY < terrainHeight - 1)
        return VOXEL_DIRT;
    if (worldY < terrainHeight)
        return VOXEL_GRASS;
    if (worldY <= WATER_LEVEL && worldY >= terrainHeight)
        return VOXEL_WATER;
    return VOXEL_AIR;
}

// Keep the old function for backward compatibility but mark it as expensive
VoxelID VoxelChunk::generateExpectedVoxel(int x, int y, int z) const
{
    // Redirect to cached version
    return generateExpectedVoxelFromCache(x, y, z);
}

bool VoxelChunk::isInBounds(int x, int y, int z) const
{
    return x >= 0 && x < SIZE && y >= 0 && y < HEIGHT && z >= 0 && z < SIZE;
}

bool VoxelChunk::isInBounds(const glm::ivec3 &pos) const
{
    return isInBounds(pos.x, pos.y, pos.z);
}

void VoxelChunk::buildMesh()
{
    if (!mesh)
    {
        return;
    }

    mesh->buildMesh(*this);
    is_mesh_dirty = false;
}

bool VoxelChunk::needsMeshRebuild() const
{
    return is_mesh_dirty || !mesh || !mesh->isBuilt();
}