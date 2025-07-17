#pragma once

#include "world/chunk_manager.h"
#include "world/coordinate.h"
#include "world/voxel_data.h"
#include "shader.h"
#include "camera.h"
#include "model.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <memory>

/**
 * @brief Main voxel world class that handles chunk rendering and management
 */
class VoxelWorld {
public:
    VoxelWorld();
    ~VoxelWorld();

    /**
     * @brief Initialize the voxel world rendering system
     */
    void initialize();

    /**
     * @brief Initialize basic voxel types
     */
    void initializeVoxelTypes();
    
    /**
     * @brief Load block models from the models directory
     */
    void loadBlockModels();

    /**
     * @brief Update the world based on camera position
     * @param cameraPosition Current camera position
     */
    void update(const glm::vec3& cameraPosition);

    /**
     * @brief Render all visible chunks
     * @param shader Shader to use for rendering
     * @param view View matrix
     * @param projection Projection matrix
     */
    void render(Shader& shader, const glm::mat4& view, const glm::mat4& projection);

    /**
     * @brief Generate terrain for a chunk
     * @param chunkPos Position of the chunk to generate
     */
    void generateChunk(const ChunkPosition& chunkPos);

    /**
     * @brief Generate terrain for an entire chunk column (X, Z)
     * @param chunkX X coordinate of the chunk column
     * @param chunkZ Z coordinate of the chunk column
     */
    void generateChunkColumn(int chunkX, int chunkZ);

    /**
     * @brief Get voxel at world position
     * @param position World position
     * @return voxel_t The voxel type
     */
    voxel_t getVoxel(const VoxelPosition& position) const;

    /**
     * @brief Set voxel at world position
     * @param position World position
     * @param voxel Voxel type to set
     */
    void setVoxel(const VoxelPosition& position, voxel_t voxel);

private:
    ChunkManager m_chunkManager;
    VoxelDataManager m_voxelDataManager;
    int m_renderDistance;
    int m_worldSeed;
    int m_worldSize;
    
    // Model loading and management
    std::unordered_map<std::string, std::unique_ptr<Model>> m_blockModels;
    
    // Current chunk being processed (for face culling)
    const Chunk* m_currentChunk;
    ChunkPosition m_currentChunkPos;
    
    // Chunk mesh data
    struct ChunkMesh {
        GLuint VAO, VBO, EBO;
        int indexCount;
        bool needsUpdate;
        ChunkPosition position;
    };
    
    std::unordered_map<ChunkPosition, ChunkMesh, ChunkPositionHash> m_chunkMeshes;
    
    /**
     * @brief Generate mesh for a chunk
     * @param chunkPos Position of the chunk
     */
    void generateChunkMesh(const ChunkPosition& chunkPos);
    
    /**
     * @brief Update mesh for a chunk
     * @param chunkPos Position of the chunk
     */
    void updateChunkMesh(const ChunkPosition& chunkPos);
    
    /**
     * @brief Generate simple terrain for a chunk
     * @param chunk Reference to the chunk to fill
     * @param chunkPos Position of the chunk
     */
    void generateTerrain(Chunk& chunk, const ChunkPosition& chunkPos);
    
    /**
     * @brief Create cube vertices for a voxel at local position
     * @param localPos Local position within chunk
     * @param voxelType Type of voxel
     * @param voxelData Data for the voxel (mesh style, textures, etc.)
     * @param vertices Output vertex data
     * @param indices Output index data
     * @param vertexOffset Current vertex offset for indices
     */
    void createVoxelMesh(const VoxelPosition& localPos, voxel_t voxelType, const VoxelData& voxelData,
                        std::vector<float>& vertices, std::vector<unsigned int>& indices, 
                        unsigned int& vertexOffset);

    /**
     * @brief Create cube mesh for solid blocks
     */
    void createCubeMesh(const VoxelPosition& localPos, voxel_t voxelType, const VoxelData& voxelData,
                       std::vector<float>& vertices, std::vector<unsigned int>& indices, 
                       unsigned int& vertexOffset);

    /**
     * @brief Create cross mesh for plants/vegetation
     */
    void createCrossMesh(const VoxelPosition& localPos, voxel_t voxelType, const VoxelData& voxelData,
                        std::vector<float>& vertices, std::vector<unsigned int>& indices, 
                        unsigned int& vertexOffset);
                        
    /**
     * @brief Create mesh from loaded 3D model
     */
    void createModelMesh(const VoxelPosition& localPos, voxel_t voxelType, const VoxelData& voxelData,
                        std::vector<float>& vertices, std::vector<unsigned int>& indices, 
                        unsigned int& vertexOffset);
    
    /**
     * @brief Check if a face should be rendered (is the adjacent voxel transparent?)
     * @param localPos Local position within chunk
     * @param direction Direction to check (0=left, 1=right, 2=bottom, 3=top, 4=back, 5=front)
     * @return true if face should be rendered
     */
    bool shouldRenderFace(const VoxelPosition& localPos, int direction);
    
    ChunkPosition m_lastCameraChunk;
};
