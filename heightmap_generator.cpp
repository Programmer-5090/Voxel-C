#include "heightmap_generator.h"
#include "voxel world/voxel_noise.h"
#include <iostream>
#include <algorithm>
#include <filesystem>

// We'll use stb_image_write for PNG output
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "includes/glfw-3.4/glfw-3.4/deps/stb_image_write.h"

void HeightmapGenerator::generateAllHeightmaps(uint32_t seed, int width, int height)
{
    VoxelNoise noise(seed);
    float scale = 0.005f;

    std::cout << "Generating heightmaps with seed: " << seed << std::endl;

    // Create heightmaps directory if it doesn't exist
    std::filesystem::create_directories("heightmaps");

    // Generate Continental heightmap
    auto continental = generateContinentalHeightmap(noise, width, height, scale);
    saveHeightmapAsPNG(continental, width, height, "heightmaps/continental_noise.png");

    // Generate Erosion heightmap
    auto erosion = generateErosionHeightmap(noise, width, height, scale);
    saveHeightmapAsPNG(erosion, width, height, "heightmaps/erosion_noise.png");

    // Generate Peaks and Valleys heightmap
    auto peaksValleys = generatePeaksValleysHeightmap(noise, width, height, scale);
    saveHeightmapAsPNG(peaksValleys, width, height, "heightmaps/peaks_valleys_noise.png");

    // Generate Simplex noise heightmap
    auto simplex = generateSimplexHeightmap(noise, width, height, scale);
    saveHeightmapAsPNG(simplex, width, height, "heightmaps/simplex_noise.png");

    // Generate Fractal noise heightmap
    auto fractal = generateFractalHeightmap(noise, width, height, scale);
    saveHeightmapAsPNG(fractal, width, height, "heightmaps/fractal_noise.png");

    // Generate Final terrain heightmap (using splines)
    auto finalTerrain = generateFinalTerrainHeightmap(noise, width, height, scale);
    saveHeightmapAsPNG(finalTerrain, width, height, "heightmaps/final_terrain.png");

    std::cout << "All heightmaps generated successfully!" << std::endl;
}

std::vector<float> HeightmapGenerator::generateContinentalHeightmap(const VoxelNoise &noise, int width, int height, float scale)
{
    std::vector<float> heightmap(width * height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float continental = noise.getContinentalness(x * scale, y * scale);
            // Normalize from [-1, 1] to [0, 1]
            heightmap[y * width + x] = (continental + 1.0f) * 0.5f;
        }
    }
    return heightmap;
}

std::vector<float> HeightmapGenerator::generateErosionHeightmap(const VoxelNoise &noise, int width, int height, float scale)
{
    std::vector<float> heightmap(width * height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float erosion = noise.getErosion(x * scale, y * scale);
            // Normalize from [-1, 1] to [0, 1]
            heightmap[y * width + x] = (erosion + 1.0f) * 0.5f;
        }
    }
    return heightmap;
}

std::vector<float> HeightmapGenerator::generatePeaksValleysHeightmap(const VoxelNoise &noise, int width, int height, float scale)
{
    std::vector<float> heightmap(width * height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float peaksValleys = const_cast<VoxelNoise &>(noise).getPeaksandValleysGenerator(x * scale, y * scale);
            // Normalize from [-1, 1] to [0, 1]
            heightmap[y * width + x] = (peaksValleys + 1.0f) * 0.5f;
        }
    }
    return heightmap;
}

std::vector<float> HeightmapGenerator::generateSimplexHeightmap(const VoxelNoise &noise, int width, int height, float scale)
{
    std::vector<float> heightmap(width * height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float simplex = noise.sample2D(x * scale, y * scale);
            // Normalize from [-1, 1] to [0, 1]
            heightmap[y * width + x] = (simplex + 1.0f) * 0.5f;
        }
    }
    return heightmap;
}

std::vector<float> HeightmapGenerator::generateFractalHeightmap(const VoxelNoise &noise, int width, int height, float scale)
{
    std::vector<float> heightmap(width * height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float fractal = noise.fractal2D(x * scale, y * scale);
            // Normalize from [-1, 1] to [0, 1]
            heightmap[y * width + x] = (fractal + 1.0f) * 0.5f;
        }
    }
    return heightmap;
}

std::vector<float> HeightmapGenerator::generateFinalTerrainHeightmap(const VoxelNoise &noise, int width, int height, float scale)
{
    std::vector<float> heightmap(width * height);

    // Use the same splines as in your terrain generation
    std::vector<SplinePoint> continentalSpline = {
        {-1.0f, 30.0f}, // Ocean floors
        {-0.5f, 50.0f}, // Coastal areas
        {0.0f, 80.0f},  // Plains
        {0.3f, 100.0f}, // Hills
        {0.6f, 130.0f}, // Mountains
        {1.0f, 160.0f}  // High peaks
    };

    std::vector<SplinePoint> erosionSpline = {
        {-1.0f, 0.0f}, // No erosion effect
        {0.0f, 10.0f}, // Light erosion
        {0.5f, 25.0f}, // Medium erosion
        {1.0f, 40.0f}  // Heavy erosion (carves valleys)
    };

    float minHeight = 1000.0f, maxHeight = -1000.0f;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float continental = noise.getContinentalness(x * scale, y * scale);
            float erosion = noise.getErosion(x * scale, y * scale);

            float baseHeight = const_cast<VoxelNoise &>(noise).evalSpline(continentalSpline, continental);
            float erosionEffect = const_cast<VoxelNoise &>(noise).evalSpline(erosionSpline, erosion);
            float terrainHeight = baseHeight - erosionEffect;

            heightmap[y * width + x] = terrainHeight;
            minHeight = std::min(minHeight, terrainHeight);
            maxHeight = std::max(maxHeight, terrainHeight);
        }
    }

    // Normalize to [0, 1] range
    for (float &val : heightmap)
    {
        val = (val - minHeight) / (maxHeight - minHeight);
    }

    return heightmap;
}

void HeightmapGenerator::saveHeightmapAsPNG(const std::vector<float> &heightmap, int width, int height, const std::string &filename)
{
    auto grayscale = floatToGrayscale(heightmap);

    if (stbi_write_png(filename.c_str(), width, height, 1, grayscale.data(), width))
    {
        std::cout << "Saved heightmap: " << filename << std::endl;
    }
    else
    {
        std::cout << "Failed to save heightmap: " << filename << std::endl;
    }
}

std::vector<unsigned char> HeightmapGenerator::floatToGrayscale(const std::vector<float> &heightmap)
{
    std::vector<unsigned char> grayscale(heightmap.size());

    for (size_t i = 0; i < heightmap.size(); i++)
    {
        // Clamp to [0, 1] and convert to [0, 255]
        float val = std::max(0.0f, std::min(1.0f, heightmap[i]));
        grayscale[i] = static_cast<unsigned char>(val * 255.0f);
    }

    return grayscale;
}
