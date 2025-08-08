#ifndef CHUNK_MESH_H
#define CHUNK_MESH_H

#include "voxel_types.h"
#include <glm/glm/glm.hpp>
#include <vector>
#include <glad/glad/glad.h>

// Forward declaration
class VoxelChunk;

// Vertex structure for voxel rendering
struct VoxelVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    float textureId;
    float debugFlag; // NEW: 0 = normal, 1 = should be culled but isn't

    VoxelVertex(const glm::vec3 &pos, const glm::vec3 &norm, const glm::vec2 &tex, float texId, float debug)
        : position(pos), normal(norm), texCoord(tex), textureId(texId), debugFlag(debug) {}
};

class ChunkMesh
{
public:
    // OpenGL buffer objects
    GLuint VAO, VBO, EBO;

    // Mesh data
    std::vector<VoxelVertex> vertices;
    std::vector<GLuint> indices;

    // State tracking
    bool is_built;
    bool is_uploaded;
    size_t vertex_count;
    size_t index_count;

public:
    ChunkMesh();
    ~ChunkMesh();

    // Mesh building
    void buildMesh(const VoxelChunk &chunk);
    void clear();

    // OpenGL operations
    void uploadToGPU();
    void render() const;

    // State queries
    bool isEmpty() const { return vertices.empty(); }
    bool isBuilt() const { return is_built; }
    bool isUploaded() const { return is_uploaded; }
    bool hasData() const { return is_built && !vertices.empty(); } // Check if mesh has vertex data (but hasn't been uploaded yet)

private:
    // Face generation
    void addFace(const glm::vec3 &position, int face_direction, VoxelID voxel_type, int chunk_x, int chunk_y, int chunk_z);



    // OpenGL cleanup
    void cleanupGL();

    // Face vertex data
    static const glm::vec3 FACE_VERTICES[6][4];
    static const glm::vec3 FACE_NORMALS[6];
    static const glm::vec2 FACE_TEX_COORDS[4];
    const VoxelChunk* current_chunk; // Add this member variable

    // Optimized versions
    bool shouldRenderFaceOptimized(const VoxelChunk &chunk, int x, int y, int z, int face_direction, VoxelID current_voxel) const;
    void addFaceOptimized(const glm::vec3 &position, int face_direction, VoxelID voxel_type, int chunk_x, int chunk_y, int chunk_z);
};

#endif // CHUNK_MESH_H
