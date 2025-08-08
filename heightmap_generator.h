#ifndef HEIGHTMAP_GENERATOR_H
#define HEIGHTMAP_GENERATOR_H

#include "voxel world/voxel_noise.h"
#include <vector>
#include <string>

class HeightmapGenerator
{
public:
    static void generateAllHeightmaps(uint32_t seed, int width = 512, int height = 512);

private:
    static void saveHeightmapAsPNG(const std::vector<float> &heightmap, int width, int height, const std::string &filename);
    static std::vector<unsigned char> floatToGrayscale(const std::vector<float> &heightmap);
    static std::vector<float> generateContinentalHeightmap(const VoxelNoise &noise, int width, int height, float scale);
    static std::vector<float> generateErosionHeightmap(const VoxelNoise &noise, int width, int height, float scale);
    static std::vector<float> generatePeaksValleysHeightmap(const VoxelNoise &noise, int width, int height, float scale);
    static std::vector<float> generateFinalTerrainHeightmap(const VoxelNoise &noise, int width, int height, float scale);
    static std::vector<float> generateSimplexHeightmap(const VoxelNoise &noise, int width, int height, float scale);
    static std::vector<float> generateFractalHeightmap(const VoxelNoise &noise, int width, int height, float scale);
};

#endif // HEIGHTMAP_GENERATOR_H
