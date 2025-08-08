#ifndef VOXEL_RENDERER_H
#define VOXEL_RENDERER_H

#include "voxel_world.h"
#include "voxel_types.h"
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtc/type_ptr.hpp>
#include <glad/glad/glad.h>
#include <memory>
#include <string>

#include <iostream>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

// Forward declarations
class Camera;
class Shader;

class VoxelRenderer
{
private:
    std::unique_ptr<VoxelWorld> world;
    std::unique_ptr<Shader> shader;

    // Rendering statistics
    mutable size_t chunks_rendered_last_frame;
    mutable size_t vertices_rendered_last_frame;

    // Batching and optimization
    mutable std::vector<glm::mat4> instance_matrices;
    mutable std::vector<VoxelChunk *> batch_chunks;
    GLuint instance_vbo;

    // Performance tracking
    mutable float last_frame_time;
    mutable size_t total_triangles_rendered;

    // Texture management
    GLuint block_texture_atlas;

    // Water animation
    int water_frame_start;
    int water_frame_count;
    float water_animation_time;

    // Shader uniform locations
    GLint uniform_model;
    GLint uniform_view;
    GLint uniform_projection;
    GLint uniform_texture_atlas;
    GLint uniform_time;
    GLint uniform_render_pass; // ADD THIS LINE

public:
    explicit VoxelRenderer(uint32_t seed, int render_distance = 16);
    ~VoxelRenderer();

    // Initialization
    bool initialize();
    void cleanup();

    // Main update and render loop
    void update(const Camera &camera);
    void render(const Camera &camera, const glm::mat4 &projection);

    // Voxel manipulation
    VoxelID getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, VoxelID voxel);

    // Statistics
    size_t getChunksRendered() const { return chunks_rendered_last_frame; }
    size_t getVerticesRendered() const { return vertices_rendered_last_frame; }
    size_t getTotalTriangles() const { return total_triangles_rendered; }
    float getLastFrameTime() const { return last_frame_time; }
    size_t getLoadedChunkCount() const;

    // Settings
    void setRenderDistance(int distance);
    int getRenderDistance() const;

private:
    // Initialization helpers
    bool loadShaders();
    bool loadTextures();
    void setupShaderUniforms();

    // Rendering helpers
    void renderChunk(const VoxelChunk &chunk, const glm::mat4 &model_matrix);
    glm::mat4 getChunkModelMatrix(const glm::ivec3 &chunk_pos) const;

    // Animation functions
    int getCurrentWaterTextureIndex() const;

    // Culling and LOD
    bool isChunkInFrustum(const glm::ivec3 &chunk_pos, const Camera &camera, const glm::mat4 &projection) const;
    int getChunkLOD(const glm::ivec3 &chunk_pos, const Camera &camera) const;

    // For multithreading
    std::vector<std::thread> mesh_workers;
    std::priority_queue<std::pair<float, VoxelChunk *>,
                        std::vector<std::pair<float, VoxelChunk *>>,
                        std::greater<std::pair<float, VoxelChunk *>>>
        chunks_to_mesh_queue; // Min-heap (nearest first)
    std::queue<VoxelChunk *> chunks_to_upload_queue;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop_workers = false;

    void workerLoop();
};

#endif // VOXEL_RENDERER_H
