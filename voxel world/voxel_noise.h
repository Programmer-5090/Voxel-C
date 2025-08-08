#ifndef VOXEL_NOISE_H
#define VOXEL_NOISE_H

#include <cstdint>
#include <FastNoise/FastNoise.h>
#include <cmath>
#include <vector>
#include <iostream>

struct SplinePoint
{
    float input;  // Noise value
    float output; // terrain height (y)
};

class VoxelNoise
{
private:
    uint32_t seed;

    // FastNoise generators
    FastNoise::SmartNode<FastNoise::Generator> simplexGenerator;
    FastNoise::SmartNode<FastNoise::Generator> perlinGenerator;
    FastNoise::SmartNode<FastNoise::FractalFBm> fractalGenerator;

    FastNoise::SmartNode<FastNoise::FractalFBm> continentalGenerator;
    FastNoise::SmartNode<FastNoise::FractalFBm> erosionGenerator;
    FastNoise::SmartNode<FastNoise::FractalFBm> peaksValleysGenerator;

    // Simple hash function for generating pseudo-random values
    static inline uint32_t hash(uint32_t x)
    {
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        return x;
    }

    // Hash function for 2D coordinates
    static inline uint32_t hash2D(int x, int y, uint32_t seed)
    {
        return hash(x + seed) ^ hash(y + seed * 2);
    }

    // Hash function for 3D coordinates
    static inline uint32_t hash3D(int x, int y, int z, uint32_t seed)
    {
        return hash(x + seed) ^ hash(y + seed * 2) ^ hash(z + seed * 3);
    }

    // Convert hash to float in range [0, 1]
    static inline float hashToFloat(uint32_t h)
    {
        return (h & 0xFFFFFF) / float(0xFFFFFF);
    }

    // Linear interpolation
    static inline float lerp(float a, float b, float t)
    {
        return a + t * (b - a);
    }

    // Smooth interpolation (smoothstep)
    static inline float smoothstep(float t)
    {
        return t * t * (3.0f - 2.0f * t);
    }

public:
    explicit VoxelNoise(uint32_t seed) : seed(seed)
    {
        // Initialize FastNoise generators
        simplexGenerator = FastNoise::New<FastNoise::Simplex>();
        perlinGenerator = FastNoise::New<FastNoise::Perlin>();

        // Create fractal generator for more complex patterns
        fractalGenerator = FastNoise::New<FastNoise::FractalFBm>();
        fractalGenerator->SetSource(simplexGenerator);
        fractalGenerator->SetOctaveCount(4);
        fractalGenerator->SetLacunarity(2.0f);
        fractalGenerator->SetGain(0.5f);

        // Continental Generator (Large, smooth features)
        continentalGenerator = FastNoise::New<FastNoise::FractalFBm>();
        continentalGenerator->SetSource(simplexGenerator);
        continentalGenerator->SetOctaveCount(3);
        continentalGenerator->SetLacunarity(1.5f);
        continentalGenerator->SetGain(0.5f);

        // Erosion Generator (Smaller, rougher features)
        erosionGenerator = FastNoise::New<FastNoise::FractalFBm>();
        erosionGenerator->SetSource(simplexGenerator);
        erosionGenerator->SetOctaveCount(4);
        erosionGenerator->SetLacunarity(2.0f);
        erosionGenerator->SetGain(0.5f);

        // Peaks & Valleys Generator (medium scale)
        peaksValleysGenerator = FastNoise::New<FastNoise::FractalFBm>();
        peaksValleysGenerator->SetSource(simplexGenerator);
        peaksValleysGenerator->SetOctaveCount(4);
        peaksValleysGenerator->SetLacunarity(2.0f);
        peaksValleysGenerator->SetGain(0.5f);
    }

    // Sample noise at 2D coordinates using FastNoise
    float sample2D(float x, float y) const
    {
        return simplexGenerator->GenSingle2D(x, y, seed);
    }

    // Sample noise at 3D coordinates using FastNoise
    float sample3D(float x, float y, float z) const
    {
        return simplexGenerator->GenSingle3D(x, y, z, seed);
    }

    // Multi-octave noise (fractal noise) using FastNoise
    float fractal2D(float x, float y, int octaves = 4, float frequency = 1.0f, float amplitude = 1.0f, float lacunarity = 2.0f, float persistence = 0.5f) const
    {
        return fractalGenerator->GenSingle2D(x * frequency, y * frequency, seed);
    }

    float getContinentalness(float x, float y) const
    {
        return continentalGenerator->GenSingle2D(x, y, seed);
    }

    float getErosion(float x, float y) const
    {
        return erosionGenerator->GenSingle2D(x, y, seed);
    }

    float getPeaksandValleysGenerator(float x, float y){
        return peaksValleysGenerator->GenSingle2D(x, y, seed);
    }

    // Multi-octave 3D noise using FastNoise
    float fractal3D(float x, float y, float z, int octaves = 4, float frequency = 1.0f, float amplitude = 1.0f, float lacunarity = 2.0f, float persistence = 0.5f) const
    {
        return fractalGenerator->GenSingle3D(x * frequency, y * frequency, z * frequency, seed);
    }

    float evalSpline(const std::vector<SplinePoint> &spline, float t)
    {
        // Clamp t to the spline's input range
        if (t <= spline.front().input)
            return spline.front().output;
        if (t >= spline.back().input)
            return spline.back().output;

        // Find the two points surrounding t
        for (size_t i = 0; i < spline.size() - 1; ++i)
        {
            if (t >= spline[i].input && t <= spline[i + 1].input)
            {
                float localT = (t - spline[i].input) / (spline[i + 1].input - spline[i].input);
                return VoxelNoise::lerp(spline[i].output, spline[i + 1].output, localT);
            }
        }
        return 0.0f; // Should never happen
    }

    // Generate height map using FastNoise2
    std::vector<float> generateHeightMap(int width, int height, float scale = 0.005f) const
    {
        std::vector<float> heightMap(width * height);

        int index = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                // Scale the coordinates for better noise patterns
                float xCoord = (float)x * scale;
                float yCoord = (float)y * scale;

                // Generate fractal noise for more interesting terrain
                float noiseValue = fractalGenerator->GenSingle2D(xCoord, yCoord, seed);

                // Normalize from [-1, 1] to [0, 1]
                heightMap[index] = (noiseValue + 1.0f) * 0.5f;
                index++;
            }
        }

        std::cout << "Generated fractal height map with " << width << "x" << height << " pixels" << std::endl;
        return heightMap;
    }
};

#endif // VOXEL_NOISE_H
