#include "voxel_world.h"
#include "world/world_constants.h"
#include "world/terrain_generation.h"
#include "world/coordinate.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/noise.hpp>
#include <glad/glad.h>

VoxelWorld::VoxelWorld() : m_renderDistance(4), m_lastCameraChunk({0, 0, 0}), m_currentChunk(nullptr), m_worldSeed(12345), m_worldSize(64) {
    // Initialize basic voxel types
    initializeVoxelTypes();
    // Load block models
    loadBlockModels();
}

VoxelWorld::~VoxelWorld() {
    // Clean up OpenGL resources
    for (auto& [pos, mesh] : m_chunkMeshes) {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
        glDeleteBuffers(1, &mesh.EBO);
    }
}

void VoxelWorld::initialize() {
    // Initialize any necessary resources
    std::cout << "Voxel World initialized" << std::endl;
}

void VoxelWorld::initializeVoxelTypes() {
    // Add basic voxel types
    VoxelData air = {0, "air", "", "", "", "", 0, 0, 0, VoxelMeshStyle::None, VoxelType::Gas, false};
    VoxelData stone = {1, "stone", "stone", "stone", "stone", "models/stone block/cube.obj", 0, 0, 0, VoxelMeshStyle::Voxel, VoxelType::Solid, true};
    VoxelData grass = {2, "grass", "grass", "grass", "dirt", "models/grass block/cube.obj", 0, 0, 0, VoxelMeshStyle::Voxel, VoxelType::Solid, true};
    VoxelData dirt = {3, "dirt", "dirt", "dirt", "dirt", "models/dirt block/cube.obj", 0, 0, 0, VoxelMeshStyle::Voxel, VoxelType::Solid, true};
    VoxelData sand = {4, "sand", "sand", "sand", "sand", "models/sand block/cube.obj", 0, 0, 0, VoxelMeshStyle::Voxel, VoxelType::Solid, true};
    VoxelData water = {5, "water", "water", "water", "water", "", 0, 0, 0, VoxelMeshStyle::Voxel, VoxelType::Fluid, false};
    
    // Example: Add a flower voxel that uses cross mesh style
    VoxelData flower = {6, "flower", "flower", "flower", "flower", "", 0, 0, 0, VoxelMeshStyle::Cross, VoxelType::Flora, false};
    
    m_voxelDataManager.addVoxelData(air);
    m_voxelDataManager.addVoxelData(stone);
    m_voxelDataManager.addVoxelData(grass);
    m_voxelDataManager.addVoxelData(dirt);
    m_voxelDataManager.addVoxelData(sand);
    m_voxelDataManager.addVoxelData(water);
    m_voxelDataManager.addVoxelData(flower);
    
    // Initialize common voxel types lookup
    m_voxelDataManager.initCommonVoxelTypes();
}

void VoxelWorld::update(const glm::vec3& cameraPosition) {
    ChunkPosition currentChunk = worldToChunkPosition(cameraPosition);
    
    // Only update if camera moved to a different chunk
    if (currentChunk.x != m_lastCameraChunk.x || 
        currentChunk.y != m_lastCameraChunk.y || 
        currentChunk.z != m_lastCameraChunk.z) {
        
        m_lastCameraChunk = currentChunk;
        
        // Generate chunks in render distance
        for (int x = -m_renderDistance; x <= m_renderDistance; x++) {
            for (int z = -m_renderDistance; z <= m_renderDistance; z++) {
                ChunkPosition columnPos = {
                    currentChunk.x + x,
                    0, // Start from ground level
                    currentChunk.z + z
                };
                
                // Check if this chunk column needs to be generated
                if (!m_chunkManager.hasChunk(columnPos)) {
                    generateChunkColumn(columnPos.x, columnPos.z);
                }
            }
        }
    }
}

void VoxelWorld::render(Shader& shader, const glm::mat4& view, const glm::mat4& projection) {
    shader.use();
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);
    
    // Render all chunk meshes
    for (auto& [pos, mesh] : m_chunkMeshes) {
        if (mesh.indexCount > 0) {
            // Calculate world position of chunk
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(
                pos.x * CHUNK_SIZE,
                pos.y * CHUNK_SIZE,
                pos.z * CHUNK_SIZE
            ));
            shader.setMat4("model", model);
            
            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
        }
    }
    glBindVertexArray(0);
}

void VoxelWorld::generateChunk(const ChunkPosition& chunkPos) {
    // This method is now only used for individual chunk generation
    // Most terrain generation should go through generateChunkColumn
    Chunk& chunk = m_chunkManager.addChunk(chunkPos);
    generateChunkMesh(chunkPos);
}

void VoxelWorld::generateChunkColumn(int chunkX, int chunkZ) {
    // Use the advanced terrain generation system
    std::vector<ChunkPosition> generatedChunks = ::generateTerrain(
        m_chunkManager, 
        chunkX, 
        chunkZ, 
        m_voxelDataManager, 
        m_worldSeed, 
        m_worldSize
    );
    
    // Generate meshes for all the chunks that were created
    for (const ChunkPosition& pos : generatedChunks) {
        generateChunkMesh(pos);
    }
}

void VoxelWorld::generateTerrain(Chunk& chunk, const ChunkPosition& chunkPos) {
    // This method should be removed since we're using the external terrain generation
    // The terrain_generation.cpp system handles this better
}

void VoxelWorld::generateChunkMesh(const ChunkPosition& chunkPos) {
    if (!m_chunkManager.hasChunk(chunkPos)) return;
    
    const Chunk& chunk = m_chunkManager.getChunk(chunkPos);
    
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int vertexOffset = 0;
    
    // Store current chunk info for face culling
    m_currentChunk = &chunk;
    m_currentChunkPos = chunkPos;
    
    // Generate mesh for each voxel in the chunk
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                VoxelPosition localPos = {x, y, z};
                voxel_t voxel = chunk.qGetVoxel(localPos);
                
                // Skip air voxels
                if (voxel == static_cast<voxel_t>(CommonVoxel::Air)) continue;
                
                // Get voxel data to determine mesh style
                const VoxelData& voxelData = m_voxelDataManager.getVoxelData(voxel);
                
                // Skip voxels with no mesh
                if (voxelData.meshStyle == VoxelMeshStyle::None) continue;
                
                createVoxelMesh(localPos, voxel, voxelData, vertices, indices, vertexOffset);
            }
        }
    }
    
    // Create or update mesh
    ChunkMesh& mesh = m_chunkMeshes[chunkPos];
    mesh.position = chunkPos;
    mesh.indexCount = static_cast<int>(indices.size());
    
    // Generate OpenGL buffers if they don't exist
    if (mesh.VAO == 0) {
        glGenVertexArrays(1, &mesh.VAO);
        glGenBuffers(1, &mesh.VBO);
        glGenBuffers(1, &mesh.EBO);
    }
    
    // Upload mesh data
    glBindVertexArray(mesh.VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

void VoxelWorld::createVoxelMesh(const VoxelPosition& localPos, voxel_t voxelType, const VoxelData& voxelData,
                                std::vector<float>& vertices, std::vector<unsigned int>& indices, 
                                unsigned int& vertexOffset) {
    
    switch (voxelData.meshStyle) {
        case VoxelMeshStyle::Voxel:
            createCubeMesh(localPos, voxelType, voxelData, vertices, indices, vertexOffset);
            break;
        case VoxelMeshStyle::Cross:
            createCrossMesh(localPos, voxelType, voxelData, vertices, indices, vertexOffset);
            break;
        case VoxelMeshStyle::Model:
            createModelMesh(localPos, voxelType, voxelData, vertices, indices, vertexOffset);
            break;
        case VoxelMeshStyle::None:
            // No mesh to create
            break;
        default:
            // Default to cube mesh
            createCubeMesh(localPos, voxelType, voxelData, vertices, indices, vertexOffset);
            break;
    }
}

void VoxelWorld::createCubeMesh(const VoxelPosition& localPos, voxel_t voxelType, const VoxelData& voxelData,
                               std::vector<float>& vertices, std::vector<unsigned int>& indices, 
                               unsigned int& vertexOffset) {
    
    float x = static_cast<float>(localPos.x);
    float y = static_cast<float>(localPos.y);
    float z = static_cast<float>(localPos.z);
    
    // Define the 6 faces with their direction indices
    // 0=left(-X), 1=right(+X), 2=bottom(-Y), 3=top(+Y), 4=back(-Z), 5=front(+Z)
    
    // Check each face and only add it if it should be rendered
    unsigned int currentVertexOffset = vertexOffset;
    
    // Front face (+Z)
    if (shouldRenderFace(localPos, 5)) {
        std::vector<float> faceVertices = {
            x - 0.5f, y - 0.5f, z + 0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
            x + 0.5f, y - 0.5f, z + 0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
            x + 0.5f, y + 0.5f, z + 0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
            x - 0.5f, y + 0.5f, z + 0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        };
        std::vector<unsigned int> faceIndices = {
            0, 1, 2, 2, 3, 0
        };
        
        vertices.insert(vertices.end(), faceVertices.begin(), faceVertices.end());
        for (unsigned int index : faceIndices) {
            indices.push_back(index + currentVertexOffset);
        }
        currentVertexOffset += 4;
    }
    
    // Back face (-Z)
    if (shouldRenderFace(localPos, 4)) {
        std::vector<float> faceVertices = {
            x - 0.5f, y - 0.5f, z - 0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
            x + 0.5f, y - 0.5f, z - 0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
            x + 0.5f, y + 0.5f, z - 0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
            x - 0.5f, y + 0.5f, z - 0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        };
        std::vector<unsigned int> faceIndices = {
            0, 1, 2, 2, 3, 0
        };
        
        vertices.insert(vertices.end(), faceVertices.begin(), faceVertices.end());
        for (unsigned int index : faceIndices) {
            indices.push_back(index + currentVertexOffset);
        }
        currentVertexOffset += 4;
    }
    
    // Left face (-X)
    if (shouldRenderFace(localPos, 0)) {
        std::vector<float> faceVertices = {
            x - 0.5f, y - 0.5f, z - 0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
            x - 0.5f, y - 0.5f, z + 0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
            x - 0.5f, y + 0.5f, z + 0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
            x - 0.5f, y + 0.5f, z - 0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        };
        std::vector<unsigned int> faceIndices = {
            0, 1, 2, 2, 3, 0
        };
        
        vertices.insert(vertices.end(), faceVertices.begin(), faceVertices.end());
        for (unsigned int index : faceIndices) {
            indices.push_back(index + currentVertexOffset);
        }
        currentVertexOffset += 4;
    }
    
    // Right face (+X)
    if (shouldRenderFace(localPos, 1)) {
        std::vector<float> faceVertices = {
            x + 0.5f, y - 0.5f, z - 0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
            x + 0.5f, y - 0.5f, z + 0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
            x + 0.5f, y + 0.5f, z + 0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
            x + 0.5f, y + 0.5f, z - 0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        };
        std::vector<unsigned int> faceIndices = {
            0, 1, 2, 2, 3, 0
        };
        
        vertices.insert(vertices.end(), faceVertices.begin(), faceVertices.end());
        for (unsigned int index : faceIndices) {
            indices.push_back(index + currentVertexOffset);
        }
        currentVertexOffset += 4;
    }
    
    // Bottom face (-Y)
    if (shouldRenderFace(localPos, 2)) {
        std::vector<float> faceVertices = {
            x - 0.5f, y - 0.5f, z - 0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
            x + 0.5f, y - 0.5f, z - 0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
            x + 0.5f, y - 0.5f, z + 0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
            x - 0.5f, y - 0.5f, z + 0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        };
        std::vector<unsigned int> faceIndices = {
            0, 1, 2, 2, 3, 0
        };
        
        vertices.insert(vertices.end(), faceVertices.begin(), faceVertices.end());
        for (unsigned int index : faceIndices) {
            indices.push_back(index + currentVertexOffset);
        }
        currentVertexOffset += 4;
    }
    
    // Top face (+Y)
    if (shouldRenderFace(localPos, 3)) {
        std::vector<float> faceVertices = {
            x - 0.5f, y + 0.5f, z - 0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
            x + 0.5f, y + 0.5f, z - 0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
            x + 0.5f, y + 0.5f, z + 0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
            x - 0.5f, y + 0.5f, z + 0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
        };
        std::vector<unsigned int> faceIndices = {
            0, 1, 2, 2, 3, 0
        };
        
        vertices.insert(vertices.end(), faceVertices.begin(), faceVertices.end());
        for (unsigned int index : faceIndices) {
            indices.push_back(index + currentVertexOffset);
        }
        currentVertexOffset += 4;
    }
    
    vertexOffset = currentVertexOffset;
}

void VoxelWorld::createCrossMesh(const VoxelPosition& localPos, voxel_t voxelType, const VoxelData& voxelData,
                                std::vector<float>& vertices, std::vector<unsigned int>& indices, 
                                unsigned int& vertexOffset) {
    
    float x = static_cast<float>(localPos.x);
    float y = static_cast<float>(localPos.y);
    float z = static_cast<float>(localPos.z);
    
    unsigned int currentVertexOffset = vertexOffset;
    
    // Create two intersecting planes to form a cross shape
    // First plane: diagonal from (-0.5, -0.5, -0.5) to (0.5, 0.5, 0.5)
    std::vector<float> plane1Vertices = {
        x - 0.5f, y - 0.5f, z - 0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
        x + 0.5f, y - 0.5f, z + 0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
        x + 0.5f, y + 0.5f, z + 0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        x - 0.5f, y + 0.5f, z - 0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
    };
    
    // Second plane: diagonal from (-0.5, -0.5, 0.5) to (0.5, 0.5, -0.5)
    std::vector<float> plane2Vertices = {
        x - 0.5f, y - 0.5f, z + 0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
        x + 0.5f, y - 0.5f, z - 0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
        x + 0.5f, y + 0.5f, z - 0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        x - 0.5f, y + 0.5f, z + 0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
    };
    
    // Indices for both planes (need to render both sides)
    std::vector<unsigned int> planeIndices = {
        0, 1, 2, 2, 3, 0,  // Front face
        2, 1, 0, 0, 3, 2   // Back face (reversed winding)
    };
    
    // Add first plane
    vertices.insert(vertices.end(), plane1Vertices.begin(), plane1Vertices.end());
    for (unsigned int index : planeIndices) {
        indices.push_back(index + currentVertexOffset);
    }
    currentVertexOffset += 4;
    
    // Add second plane
    vertices.insert(vertices.end(), plane2Vertices.begin(), plane2Vertices.end());
    for (unsigned int index : planeIndices) {
        indices.push_back(index + currentVertexOffset);
    }
    currentVertexOffset += 4;
    
    vertexOffset = currentVertexOffset;
}

bool VoxelWorld::shouldRenderFace(const VoxelPosition& localPos, int direction) {
    if (!m_currentChunk) return true; // Safety fallback
    
    // Direction offsets: 0=left(-X), 1=right(+X), 2=bottom(-Y), 3=top(+Y), 4=back(-Z), 5=front(+Z)
    VoxelPosition neighborPos = localPos;
    
    switch (direction) {
        case 0: neighborPos.x -= 1; break; // Left (-X)
        case 1: neighborPos.x += 1; break; // Right (+X)
        case 2: neighborPos.y -= 1; break; // Bottom (-Y)
        case 3: neighborPos.y += 1; break; // Top (+Y)
        case 4: neighborPos.z -= 1; break; // Back (-Z)
        case 5: neighborPos.z += 1; break; // Front (+Z)
        default: return true;
    }
    
    // Check if neighbor position is within current chunk bounds
    if (neighborPos.x >= 0 && neighborPos.x < CHUNK_SIZE &&
        neighborPos.y >= 0 && neighborPos.y < CHUNK_SIZE &&
        neighborPos.z >= 0 && neighborPos.z < CHUNK_SIZE) {
        
        // Neighbor is within current chunk
        voxel_t neighborVoxel = m_currentChunk->qGetVoxel(neighborPos);
        return neighborVoxel == static_cast<voxel_t>(CommonVoxel::Air);
    } else {
        // Neighbor is in a different chunk - need to check across chunk boundaries
        VoxelPosition worldPos = {
            m_currentChunkPos.x * CHUNK_SIZE + neighborPos.x,
            m_currentChunkPos.y * CHUNK_SIZE + neighborPos.y,
            m_currentChunkPos.z * CHUNK_SIZE + neighborPos.z
        };
        
        // Get the chunk that contains this world position
        ChunkPosition neighborChunkPos = toChunkPosition(worldPos);
        
        // Check if the neighboring chunk exists
        if (!m_chunkManager.hasChunk(neighborChunkPos)) {
            // No neighboring chunk loaded - render the face (assume air)
            return true;
        }
        
        // Get the neighbor voxel from the neighboring chunk
        voxel_t neighborVoxel = m_chunkManager.getVoxel(worldPos);
        return neighborVoxel == static_cast<voxel_t>(CommonVoxel::Air);
    }
}

voxel_t VoxelWorld::getVoxel(const VoxelPosition& position) const {
    return m_chunkManager.getVoxel(position);
}

void VoxelWorld::setVoxel(const VoxelPosition& position, voxel_t voxel) {
    m_chunkManager.setVoxel(position, voxel);
    
    // Mark chunk mesh for update
    ChunkPosition chunkPos = toChunkPosition(position);
    auto it = m_chunkMeshes.find(chunkPos);
    if (it != m_chunkMeshes.end()) {
        it->second.needsUpdate = true;
    }
}

void VoxelWorld::loadBlockModels() {
    // Load models for each voxel type that uses the Model mesh style
    // Define the model paths for each block type
    std::vector<std::string> modelPaths = {
        "models/stone block/cube.obj",
        "models/grass block/cube.obj",
        "models/dirt block/cube.obj",
        "models/sand block/cube.obj"
    };
    
    std::cout << "Loading block models..." << std::endl;
    
    // Load each model and store it in the map
    for (const auto& modelPath : modelPaths) {
        std::cout << "Attempting to load: " << modelPath << std::endl;
        
        try {
            // Create a unique pointer to the model
            auto model = std::make_unique<Model>(modelPath.c_str());
            
            // Check if the model loaded successfully
            if (model->meshes.empty()) {
                std::cerr << "ERROR: Model loaded but has no meshes: " << modelPath << std::endl;
                continue;
            }
            
            // Store the model in the map using the path as key
            m_blockModels[modelPath] = std::move(model);
            
            std::cout << "SUCCESS: Loaded model: " << modelPath << " with " << m_blockModels[modelPath]->meshes.size() << " meshes" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Failed to load model: " << modelPath << " - " << e.what() << std::endl;
        }
    }
    
    std::cout << "Model loading complete. Loaded " << m_blockModels.size() << " models." << std::endl;
}

void VoxelWorld::createModelMesh(const VoxelPosition& localPos, voxel_t voxelType, const VoxelData& voxelData,
                                std::vector<float>& vertices, std::vector<unsigned int>& indices, 
                                unsigned int& vertexOffset) {
    
    if (voxelData.modelPath.empty()) {
        // Fallback to regular cube mesh if no model path is specified
        createCubeMesh(localPos, voxelType, voxelData, vertices, indices, vertexOffset);
        return;
    }
    
    // Check if the model is loaded
    auto modelIt = m_blockModels.find(voxelData.modelPath);
    if (modelIt == m_blockModels.end()) {
        // Model not found, fallback to cube mesh
        std::cerr << "Model not found: " << voxelData.modelPath << ", using cube mesh fallback" << std::endl;
        createCubeMesh(localPos, voxelType, voxelData, vertices, indices, vertexOffset);
        return;
    }
    
    Model* model = modelIt->second.get();
    
    // Get the position for this voxel
    float x = localPos.x;
    float y = localPos.y;
    float z = localPos.z;
    
    // Extract vertex data from the model's first mesh
    // Note: This assumes the model has at least one mesh
    if (model->meshes.empty()) {
        std::cerr << "Model has no meshes: " << voxelData.modelPath << ", using cube mesh fallback" << std::endl;
        createCubeMesh(localPos, voxelType, voxelData, vertices, indices, vertexOffset);
        return;
    }
    
    // Use the first mesh from the model
    const Mesh& modelMesh = model->meshes[0];
    
    // Convert model vertices to our vertex format and translate to voxel position
    std::vector<float> modelVertices;
    for (const auto& vertex : modelMesh.vertices) {
        // Position (translated to voxel position)
        modelVertices.push_back(vertex.Position.x + x);
        modelVertices.push_back(vertex.Position.y + y);
        modelVertices.push_back(vertex.Position.z + z);
        
        // Normal
        modelVertices.push_back(vertex.Normal.x);
        modelVertices.push_back(vertex.Normal.y);
        modelVertices.push_back(vertex.Normal.z);
        
        // Texture coordinates
        modelVertices.push_back(vertex.TexCoords.x);
        modelVertices.push_back(vertex.TexCoords.y);
    }
    
    // Copy and adjust indices
    std::vector<unsigned int> modelIndices = modelMesh.indices;
    for (auto& index : modelIndices) {
        index += vertexOffset;
    }
    
    // Add vertices and indices to the main vectors
    vertices.insert(vertices.end(), modelVertices.begin(), modelVertices.end());
    indices.insert(indices.end(), modelIndices.begin(), modelIndices.end());
    
    // Update vertex offset for next mesh
    vertexOffset += modelMesh.vertices.size();
}
