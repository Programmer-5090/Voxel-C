#include "chunk_mesh.h"
#include "voxel_chunk.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <string>

// Face vertex definitions (relative to cube center at origin)
const glm::vec3 ChunkMesh::FACE_VERTICES[6][4] = {
    // FACE_FRONT (+Z)
    {
        glm::vec3(-0.5f, -0.5f, 0.5f), // bottom-left
        glm::vec3(0.5f, -0.5f, 0.5f),  // bottom-right
        glm::vec3(0.5f, 0.5f, 0.5f),   // top-right
        glm::vec3(-0.5f, 0.5f, 0.5f)   // top-left
    },
    // FACE_BACK (-Z)
    {
        glm::vec3(0.5f, -0.5f, -0.5f),  // bottom-left
        glm::vec3(-0.5f, -0.5f, -0.5f), // bottom-right
        glm::vec3(-0.5f, 0.5f, -0.5f),  // top-right
        glm::vec3(0.5f, 0.5f, -0.5f)    // top-left
    },
    // FACE_RIGHT (+X)
    {
        glm::vec3(0.5f, -0.5f, 0.5f),  // bottom-left
        glm::vec3(0.5f, -0.5f, -0.5f), // bottom-right
        glm::vec3(0.5f, 0.5f, -0.5f),  // top-right
        glm::vec3(0.5f, 0.5f, 0.5f)    // top-left
    },
    // FACE_LEFT (-X)
    {
        glm::vec3(-0.5f, -0.5f, -0.5f), // bottom-left
        glm::vec3(-0.5f, -0.5f, 0.5f),  // bottom-right
        glm::vec3(-0.5f, 0.5f, 0.5f),   // top-right
        glm::vec3(-0.5f, 0.5f, -0.5f)   // top-left
    },
    // FACE_TOP (+Y)
    {
        glm::vec3(-0.5f, 0.5f, 0.5f), // bottom-left
        glm::vec3(0.5f, 0.5f, 0.5f),  // bottom-right
        glm::vec3(0.5f, 0.5f, -0.5f), // top-right
        glm::vec3(-0.5f, 0.5f, -0.5f) // top-left
    },
    // FACE_BOTTOM (-Y)
    {
        glm::vec3(-0.5f, -0.5f, -0.5f), // bottom-left
        glm::vec3(0.5f, -0.5f, -0.5f),  // bottom-right
        glm::vec3(0.5f, -0.5f, 0.5f),   // top-right
        glm::vec3(-0.5f, -0.5f, 0.5f)   // top-left
    }};

// Face normals
const glm::vec3 ChunkMesh::FACE_NORMALS[6] = {
    glm::vec3(0.0f, 0.0f, 1.0f),  // FACE_FRONT
    glm::vec3(0.0f, 0.0f, -1.0f), // FACE_BACK
    glm::vec3(1.0f, 0.0f, 0.0f),  // FACE_RIGHT
    glm::vec3(-1.0f, 0.0f, 0.0f), // FACE_LEFT
    glm::vec3(0.0f, 1.0f, 0.0f),  // FACE_TOP
    glm::vec3(0.0f, -1.0f, 0.0f)  // FACE_BOTTOM
};

// Texture coordinates
const glm::vec2 ChunkMesh::FACE_TEX_COORDS[4] = {
    glm::vec2(0.0f, 0.0f), // bottom-left
    glm::vec2(1.0f, 0.0f), // bottom-right
    glm::vec2(1.0f, 1.0f), // top-right
    glm::vec2(0.0f, 1.0f)  // top-left
};

ChunkMesh::ChunkMesh()
    : VAO(0), VBO(0), EBO(0), is_built(false), is_uploaded(false), vertex_count(0), index_count(0)
{
}

ChunkMesh::~ChunkMesh()
{
    cleanupGL();
}

void ChunkMesh::buildMesh(const VoxelChunk &chunk)
{
    auto total_start = std::chrono::high_resolution_clock::now();

    current_chunk = &chunk;
    clear();

    auto setup_start = std::chrono::high_resolution_clock::now();

    const VoxelID *data = chunk.voxels.data();
    int solid_voxel_count = 0;

    auto idx = [&](int x, int y, int z)
    { return x * CHUNK_HEIGHT * CHUNK_SIZE + y * CHUNK_SIZE + z; };

    // Quick count of solid voxels
    for (int i = 0; i < CHUNK_VOLUME; ++i)
    {
        if (data[i] != VOXEL_AIR)
        {
            solid_voxel_count++;
        }
    }

    // Reserve based on actual solid voxels (max 6 faces per voxel, 4 vertices per face)
    int estimated_vertices = std::min(solid_voxel_count * 24, CHUNK_VOLUME / 4);
    vertices.reserve(estimated_vertices);
    indices.reserve(estimated_vertices * 3 / 2); // Rough estimate for indices

    auto setup_end = std::chrono::high_resolution_clock::now();

    auto inLocal = [](int x, int y, int z)
    { return x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_HEIGHT && z >= 0 && z < CHUNK_SIZE; };

    auto loop_start = std::chrono::high_resolution_clock::now();
    int faces_processed = 0;
    int neighbor_lookups = 0;

    for (int x = 0; x < CHUNK_SIZE; ++x)
        for (int y = 0; y < CHUNK_HEIGHT; ++y)
            for (int z = 0; z < CHUNK_SIZE; ++z)
            {
                VoxelID voxel = data[idx(x, y, z)];
                if (voxel == VOXEL_AIR)
                    continue;

                bool voxel_transparent = VOXEL_INFO[voxel].is_transparent;
                glm::vec3 basePos(x, y, z);

                auto emitFaceIfVisible = [&](int nx, int ny, int nz, int faceDir)
                {
                    VoxelID neighborVoxel;
                    if (inLocal(nx, ny, nz))
                    {
                        neighborVoxel = data[idx(nx, ny, nz)];
                    }
                    else
                    {
                        neighbor_lookups++;
                        // Predict neighbor using cross-chunk lookup
                        neighborVoxel = chunk.getVoxelWithNeighbors(nx, ny, nz);
                    }
                    faces_processed++;
                    if (shouldRenderFaceOptimized(chunk, x, y, z, faceDir, voxel))
                        addFaceOptimized(basePos, faceDir, voxel, x, y, z);
                };

                emitFaceIfVisible(x, y, z + 1, FACE_FRONT);
                emitFaceIfVisible(x, y, z - 1, FACE_BACK);
                emitFaceIfVisible(x + 1, y, z, FACE_RIGHT);
                emitFaceIfVisible(x - 1, y, z, FACE_LEFT);
                emitFaceIfVisible(x, y + 1, z, FACE_TOP);
                emitFaceIfVisible(x, y - 1, z, FACE_BOTTOM);
            }

    auto loop_end = std::chrono::high_resolution_clock::now();

    auto finalize_start = std::chrono::high_resolution_clock::now();
    vertex_count = vertices.size();
    index_count = indices.size();
    is_built = true;
    is_uploaded = false;
    current_chunk = nullptr;
    auto finalize_end = std::chrono::high_resolution_clock::now();

    auto total_end = std::chrono::high_resolution_clock::now();

    // Calculate timings
    float setup_time = std::chrono::duration<float, std::milli>(setup_end - setup_start).count();
    float loop_time = std::chrono::duration<float, std::milli>(loop_end - loop_start).count();
    float finalize_time = std::chrono::duration<float, std::milli>(finalize_end - finalize_start).count();
    float total_time = std::chrono::duration<float, std::milli>(total_end - total_start).count();

    // Only log if total time is significant - and use thread-safe logging
    if (total_time > 5.0f)
    {
        // Create a single string to avoid interleaved output
        std::string log_message =
            "MESH BUILD TIMING for chunk (" + std::to_string(chunk.position.x) + ", " +
            std::to_string(chunk.position.y) + ", " + std::to_string(chunk.position.z) + "):\n" +
            "  Solid voxels: " + std::to_string(solid_voxel_count) + "\n" +
            "  Reserved vertices: " + std::to_string(estimated_vertices) + "\n" +
            "  Setup: " + std::to_string(setup_time) + "ms\n" +
            "  Main Loop: " + std::to_string(loop_time) + "ms\n" +
            "  Finalize: " + std::to_string(finalize_time) + "ms\n" +
            "  TOTAL: " + std::to_string(total_time) + "ms\n" +
            "  Faces processed: " + std::to_string(faces_processed) + "\n" +
            "  Neighbor lookups: " + std::to_string(neighbor_lookups) + "\n" +
            "  Vertices generated: " + std::to_string(vertex_count) + "\n" +
            "  Indices generated: " + std::to_string(index_count) + "\n";

        std::cout << log_message << std::endl;
    }
}

void ChunkMesh::clear()
{
    // Don't shrink vectors, just clear them to avoid reallocations
    vertices.clear();
    indices.clear();

    vertex_count = 0;
    index_count = 0;
    is_built = false;
    current_chunk = nullptr;
}

void ChunkMesh::uploadToGPU()
{
    if (!is_built || vertices.empty())
    {
        return;
    }

    auto upload_start = std::chrono::high_resolution_clock::now();

    // Generate buffers if needed
    if (VAO == 0)
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }

    // Bind VAO
    glBindVertexArray(VAO);

    auto buffer_upload_start = std::chrono::high_resolution_clock::now();
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VoxelVertex), vertices.data(), GL_STATIC_DRAW);

    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    auto buffer_upload_end = std::chrono::high_resolution_clock::now();

    auto attrib_setup_start = std::chrono::high_resolution_clock::now();
    // Set vertex attributes
    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)offsetof(VoxelVertex, position));
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)offsetof(VoxelVertex, normal));
    glEnableVertexAttribArray(1);

    // Texture coordinates
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)offsetof(VoxelVertex, texCoord));
    glEnableVertexAttribArray(2);

    // Texture ID
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)offsetof(VoxelVertex, textureId));
    glEnableVertexAttribArray(3);

    // Debug flag (NEW)
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)offsetof(VoxelVertex, debugFlag));
    glEnableVertexAttribArray(4);

    // Unbind
    glBindVertexArray(0);
    auto attrib_setup_end = std::chrono::high_resolution_clock::now();

    is_uploaded = true;

    auto upload_end = std::chrono::high_resolution_clock::now();

    // Calculate timing breakdown
    float buffer_time = std::chrono::duration<float, std::milli>(buffer_upload_end - buffer_upload_start).count();
    float attrib_time = std::chrono::duration<float, std::milli>(attrib_setup_end - attrib_setup_start).count();
    float total_upload_time = std::chrono::duration<float, std::milli>(upload_end - upload_start).count();

    if (total_upload_time > 3.0f) // Log uploads that take more than 3ms
    {
        std::cout << "DETAILED GPU UPLOAD TIMING:" << std::endl;
        std::cout << "  Buffer upload: " << buffer_time << "ms" << std::endl;
        std::cout << "  Attribute setup: " << attrib_time << "ms" << std::endl;
        std::cout << "  Total upload: " << total_upload_time << "ms" << std::endl;
        std::cout << "  Data size: " << (vertices.size() * sizeof(VoxelVertex) + indices.size() * sizeof(GLuint)) / 1024.0f << " KB" << std::endl;
    }
}

void ChunkMesh::render() const
{
    if (!is_uploaded || VAO == 0 || indices.empty())
    {
        return;
    }

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void ChunkMesh::cleanupGL()
{
    if (VAO != 0)
    {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO != 0)
    {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (EBO != 0)
    {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }
    is_uploaded = false;
}

bool ChunkMesh::shouldRenderFaceOptimized(const VoxelChunk &chunk, int x, int y, int z,
                                          int face_direction, VoxelID current_voxel) const
{
    // Compute neighbor position (face_direction: 0 F,1 B,2 R,3 L,4 T,5 Btm)
    int nx = x, ny = y, nz = z;
    switch (face_direction)
    {
    case FACE_FRONT:
        nz++;
        break;
    case FACE_BACK:
        nz--;
        break;
    case FACE_RIGHT:
        nx++;
        break;
    case FACE_LEFT:
        nx--;
        break;
    case FACE_TOP:
        ny++;
        break;
    case FACE_BOTTOM:
        ny--;
        break;
    }

    VoxelID neighbor_voxel = chunk.getVoxelSafe(nx, ny, nz);
    bool current_transparent = VOXEL_INFO[current_voxel].is_transparent;
    bool neighbor_transparent = VOXEL_INFO[neighbor_voxel].is_transparent;

    // Special handling for water to reduce overdraw:
    //  - Only show TOP, BOTTOM and SIDE faces when neighbor is AIR
    //  - Hides side faces against solids and other water -> "slimmer" edges
    if (current_voxel == VOXEL_WATER)
    {
        if (neighbor_voxel == VOXEL_AIR) // only expose to air
            return true;
        return false; // neighbor is water or any non-air block
    }

    // Opaque block: render face if neighbor is transparent (air or transparent type)
    if (!current_transparent)
        return neighbor_transparent;

    // Transparent (non-water) block: render unless neighbor is same type (remove internal faces)
    return current_voxel != neighbor_voxel;
}

void ChunkMesh::addFaceOptimized(const glm::vec3 &position, int face_direction, VoxelID voxel_type, int chunk_x, int chunk_y, int chunk_z)
{
    GLuint base_index = static_cast<GLuint>(vertices.size());

    // Optimize texture ID lookup with direct array access
    float texture_id = 0.0f;
    if (voxel_type < VOXEL_COUNT)
    {
        const VoxelInfo &info = VOXEL_INFO[voxel_type];
        texture_id = (face_direction == 4) ? info.texture_top : (face_direction == 5) ? info.texture_bottom
                                                                                      : info.texture_sides;
    }

    // Simplified debug flag (remove for production)
    float debug_flag = 0.0f;

    // Pre-calculate vertex data to avoid repeated calculations
    const glm::vec3 *face_verts = FACE_VERTICES[face_direction];
    const glm::vec3 &normal = FACE_NORMALS[face_direction];

    // Add vertices directly without temporary objects
    vertices.emplace_back(position + face_verts[0], normal, FACE_TEX_COORDS[0], texture_id, debug_flag);
    vertices.emplace_back(position + face_verts[1], normal, FACE_TEX_COORDS[1], texture_id, debug_flag);
    vertices.emplace_back(position + face_verts[2], normal, FACE_TEX_COORDS[2], texture_id, debug_flag);
    vertices.emplace_back(position + face_verts[3], normal, FACE_TEX_COORDS[3], texture_id, debug_flag);

    // Add indices in one go
    indices.insert(indices.end(), {base_index + 0, base_index + 1, base_index + 2,
                                   base_index + 2, base_index + 3, base_index + 0});
}
