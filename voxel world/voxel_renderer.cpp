#include "voxel_renderer.h"
#include "chunk_mesh.h"
#include "../camera.h"
#include "../shader.h"
#include "../includes/stb_image.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <chrono>
#include <string>

VoxelRenderer::VoxelRenderer(uint32_t seed, int render_distance)
    : chunks_rendered_last_frame(0), vertices_rendered_last_frame(0), block_texture_atlas(0),
      instance_vbo(0), last_frame_time(0.0f), total_triangles_rendered(0),
      uniform_model(-1), uniform_view(-1), uniform_projection(-1), uniform_texture_atlas(-1), uniform_time(-1),
      water_frame_start(0), water_frame_count(0), water_animation_time(0.0f), stop_workers(false)
{
    world = std::make_unique<VoxelWorld>(seed, render_distance);

    // Start worker threads - increased to 10 threads
    unsigned int num_threads = 10;
    std::cout << "Starting " << num_threads << " mesh worker threads" << std::endl;

    for (unsigned int i = 0; i < num_threads; ++i)
    {
        mesh_workers.emplace_back(&VoxelRenderer::workerLoop, this);
    }
}

VoxelRenderer::~VoxelRenderer()
{
    // Signal workers to stop and join them
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop_workers = true;
    }
    condition.notify_all();
    for (std::thread &worker : mesh_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    cleanup();
}

bool VoxelRenderer::initialize()
{
    std::cout << "Initializing VoxelRenderer..." << std::endl;

    // Check OpenGL context
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    if (!loadShaders())
    {
        std::cerr << "Failed to load shaders!" << std::endl;
        return false;
    }
    std::cout << "Shaders loaded successfully!" << std::endl;

    if (!loadTextures())
    {
        std::cerr << "Failed to load textures!" << std::endl;
        return false;
    }
    std::cout << "Textures loaded successfully!" << std::endl;

    setupShaderUniforms();
    std::cout << "Shader uniforms set up!" << std::endl;

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error during initialization: " << error << std::endl;
        return false;
    }

    std::cout << "VoxelRenderer initialized successfully!" << std::endl;
    return true;
}

// Add the worker loop implementation
void VoxelRenderer::workerLoop()
{
    while (true)
    {
        VoxelChunk *chunk_to_mesh;
        float chunk_distance;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this]
                           { return stop_workers || !chunks_to_mesh_queue.empty(); });
            if (stop_workers && chunks_to_mesh_queue.empty())
            {
                return;
            }

            auto chunk_pair = chunks_to_mesh_queue.top(); // Get nearest chunk
            chunks_to_mesh_queue.pop();
            chunk_distance = chunk_pair.first;
            chunk_to_mesh = chunk_pair.second;
        }

        // std::cout << "Processing chunk at distance: " << chunk_distance << std::endl; // Debug - reduced spam

        // Only process if chunk still needs meshing (avoid redundant work)
        if (!chunk_to_mesh->needsMeshRebuild())
        {
            chunk_to_mesh->setMeshing(false);
            continue;
        }

        // Time the mesh building to identify slow chunks with timeout
        auto mesh_start = std::chrono::high_resolution_clock::now();
        bool mesh_success = false;
        bool timed_out = false;

        try
        {
            chunk_to_mesh->buildMesh();
            mesh_success = true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Mesh building failed: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Mesh building failed with unknown error" << std::endl;
        }

        auto mesh_end = std::chrono::high_resolution_clock::now();
        float mesh_time = std::chrono::duration<float, std::milli>(mesh_end - mesh_start).count();

        // Check for timeout (anything over 500ms is problematic)
        if (mesh_time > 500.0f)
        {
            timed_out = true;
            std::cout << "TIMEOUT: Mesh build took " << mesh_time << "ms for chunk at ("
                      << chunk_to_mesh->position.x << ", " << chunk_to_mesh->position.y
                      << ", " << chunk_to_mesh->position.z << ") - marking chunk as problematic" << std::endl;
        }
        else if (mesh_time > 50.0f) // Log if mesh building takes more than 50ms
        {
            std::cout << "Slow mesh build: " << mesh_time << "ms for chunk at ("
                      << chunk_to_mesh->position.x << ", " << chunk_to_mesh->position.y
                      << ", " << chunk_to_mesh->position.z << ")" << std::endl;
        }

        // Only queue for upload if mesh building was successful and didn't timeout
        if (mesh_success && !timed_out)
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            chunks_to_upload_queue.push(chunk_to_mesh);
        }
        else
        {
            // Reset meshing flag so problematic chunks can be retried later (but not immediately)
            chunk_to_mesh->setMeshing(false);
        }
    }
}

void VoxelRenderer::cleanup()
{
    if (block_texture_atlas != 0)
    {
        glDeleteTextures(1, &block_texture_atlas);
        block_texture_atlas = 0;
    }

    if (instance_vbo != 0)
    {
        glDeleteBuffers(1, &instance_vbo);
        instance_vbo = 0;
    }
}

void VoxelRenderer::update(const Camera &camera)
{
    if (!world)
    {
        return;
    }

    // Update animation time (assuming 60 FPS, increment by 1/60 second)
    water_animation_time += 1.0f / 60.0f;

    // Update world based on camera position
    world->update(camera.Position);

    // --- Dispatch meshing jobs to worker threads ---
    std::vector<std::pair<float, VoxelChunk *>> chunks_needing_mesh;
    int total_chunks = 0;
    int chunks_need_mesh = 0;
    int chunks_already_meshing = 0;

    for (const auto &[chunk_pos, chunk] : world->getChunks())
    {
        total_chunks++;
        if (chunk->needsMeshRebuild())
        {
            chunks_need_mesh++;
            if (!chunk->isMeshing())
            {
                glm::vec3 chunk_world_pos = glm::vec3(
                    chunk_pos.x * CHUNK_SIZE,
                    chunk_pos.y * CHUNK_HEIGHT,
                    chunk_pos.z * CHUNK_SIZE);
                float distance = glm::distance(camera.Position, chunk_world_pos);
                chunks_needing_mesh.emplace_back(distance, chunk.get());
            }
            else
            {
                chunks_already_meshing++;
            }
        }
    }

    // Limit and sort - NEAREST FIRST for meshing priority
    const int max_chunks_to_queue_per_frame = 8;
    if (chunks_needing_mesh.size() > max_chunks_to_queue_per_frame)
    {
        chunks_needing_mesh.resize(max_chunks_to_queue_per_frame);
    }

    // Sort by distance - NEAREST FIRST (ascending order)
    std::sort(chunks_needing_mesh.begin(), chunks_needing_mesh.end()); // This is correct

    // Queue nearest chunks first
    int current_queue_size = 0;
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        current_queue_size = chunks_to_mesh_queue.size();

        if (current_queue_size < 10) // Reduced queue size to prevent backlog
        {
            for (const auto &[distance, chunk] : chunks_needing_mesh)
            {
                chunk->setMeshing(true);
                chunks_to_mesh_queue.push({distance, chunk}); // Push distance + chunk pair
                // std::cout << "Queued chunk at distance: " << distance << std::endl; // Debug - reduced spam
            }
        }
    }
    condition.notify_all();

    // --- Upload finished meshes on the main thread ---
    // Even more conservative upload budget
    auto frame_start = std::chrono::high_resolution_clock::now();
    const float max_upload_time_ms = 1.0f; // Reduced from 2ms to 1ms
    int meshes_uploaded_this_frame = 0;

    std::unique_lock<std::mutex> lock(queue_mutex);
    while (!chunks_to_upload_queue.empty() && meshes_uploaded_this_frame < 1) // Only 1 upload per frame
    {
        VoxelChunk *chunk = chunks_to_upload_queue.front();
        chunks_to_upload_queue.pop();
        lock.unlock();

        auto upload_start = std::chrono::high_resolution_clock::now();

        if (chunk->mesh && chunk->mesh->hasData())
        {
            chunk->mesh->uploadToGPU();

            auto upload_end = std::chrono::high_resolution_clock::now();
            float upload_time = std::chrono::duration<float, std::milli>(upload_end - upload_start).count();

            if (upload_time > 2.0f) // Log uploads that take more than 2ms
            {
                std::cout << "GPU UPLOAD TIMING for chunk (" << chunk->position.x << ", "
                          << chunk->position.y << ", " << chunk->position.z << "): "
                          << upload_time << "ms (vertices: " << chunk->mesh->vertex_count
                          << ", indices: " << chunk->mesh->index_count << ")" << std::endl;
            }
            meshes_uploaded_this_frame++;
        }
        chunk->setMeshing(false);

        auto upload_end = std::chrono::high_resolution_clock::now();
        float upload_time = std::chrono::duration<float, std::milli>(upload_end - upload_start).count();

        // Check if we've used up our time budget
        auto current_time = std::chrono::high_resolution_clock::now();
        float total_time = std::chrono::duration<float, std::milli>(current_time - frame_start).count();

        if (total_time >= max_upload_time_ms)
        {
            lock.lock();
            break;
        }

        lock.lock();
    }

    // Print debug info occasionally
    static int debug_counter = 0;
    if (++debug_counter % 60 == 0) // Every 60 frames (1 second at 60fps)
    {
        std::cout << "Chunks: Total=" << total_chunks
                  << " NeedMesh=" << chunks_need_mesh
                  << " Meshing=" << chunks_already_meshing
                  << " QueueSize=" << current_queue_size << std::endl;
    }
}

void VoxelRenderer::render(const Camera &camera, const glm::mat4 &projection)
{
    if (!shader || !world)
    {
        std::cerr << "Render called but shader or world is null!" << std::endl;
        return;
    }

    // Track frame timing
    auto frame_start = std::chrono::high_resolution_clock::now();

    // Reset statistics
    chunks_rendered_last_frame = 0;
    vertices_rendered_last_frame = 0;
    total_triangles_rendered = 0;

    // Use shader and set common uniforms
    shader->use();
    glm::mat4 view = camera.GetViewMatrix();
    glUniformMatrix4fv(uniform_projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(uniform_view, 1, GL_FALSE, glm::value_ptr(view));
    if (uniform_time != -1)
    {
        glUniform1f(uniform_time, water_animation_time);
    }

    // Bind texture atlas
    if (block_texture_atlas != 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, block_texture_atlas);
        glUniform1i(uniform_texture_atlas, 0);
    }

    // ========== PASS 1: OPAQUE BLOCKS ==========
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);   // Enable writing to the depth buffer
    glDisable(GL_BLEND);    // No blending needed for opaque blocks
    glEnable(GL_CULL_FACE); // Cull back-faces for performance

    glUniform1i(uniform_render_pass, 0); // Tell shader this is the opaque pass

    // Collect and sort chunks by distance for better batching
    struct ChunkDistance
    {
        float distance;
        glm::ivec3 position;
        VoxelChunk *chunk;

        bool operator<(const ChunkDistance &other) const
        {
            return distance < other.distance;
        }
    };

    std::vector<ChunkDistance> opaque_chunks;
    glm::vec3 camera_pos = camera.Position;

    for (const auto &[chunk_pos, chunk] : world->getChunks())
    {
        if (chunk->mesh && chunk->mesh->isUploaded() && !chunk->mesh->isEmpty())
        {
            // Frustum culling
            if (!isChunkInFrustum(chunk_pos, camera, projection))
                continue;

            // Calculate distance for sorting
            glm::vec3 chunk_world_pos = glm::vec3(chunk_pos.x * CHUNK_SIZE, chunk_pos.y * CHUNK_HEIGHT, chunk_pos.z * CHUNK_SIZE);
            float distance = glm::distance(camera_pos, chunk_world_pos);
            opaque_chunks.push_back({distance, chunk_pos, chunk.get()});
        }
    }

    // Sort front to back for early Z-rejection
    std::sort(opaque_chunks.begin(), opaque_chunks.end());

    for (const auto &chunk_data : opaque_chunks)
    {
        glm::mat4 model = getChunkModelMatrix(chunk_data.position);
        glUniformMatrix4fv(uniform_model, 1, GL_FALSE, glm::value_ptr(model));
        chunk_data.chunk->mesh->render();
        chunks_rendered_last_frame++;
        vertices_rendered_last_frame += chunk_data.chunk->mesh->vertex_count;
        total_triangles_rendered += chunk_data.chunk->mesh->index_count / 3;
    }

    // ========== PASS 2: TRANSPARENT BLOCKS ==========
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // IMPORTANT: Read from depth buffer but DO NOT write to it

    glUniform1i(uniform_render_pass, 1); // Tell shader this is the transparent pass

    // Collect transparent chunks and sort back to front
    std::vector<ChunkDistance> transparent_chunks;

    for (const auto &[chunk_pos, chunk] : world->getChunks())
    {
        if (chunk->mesh && chunk->mesh->isUploaded() && !chunk->mesh->isEmpty())
        {
            // For now, assume all chunks might have transparent blocks
            // TODO: Add hasTransparentBlocks() method to VoxelChunk for optimization
            if (!isChunkInFrustum(chunk_pos, camera, projection))
                continue;

            glm::vec3 chunk_world_pos = glm::vec3(chunk_pos.x * CHUNK_SIZE, chunk_pos.y * CHUNK_HEIGHT, chunk_pos.z * CHUNK_SIZE);
            float distance = glm::distance(camera_pos, chunk_world_pos);
            transparent_chunks.push_back({distance, chunk_pos, chunk.get()});
        }
    }

    // Sort back to front for proper transparency
    std::sort(transparent_chunks.rbegin(), transparent_chunks.rend());

    for (const auto &chunk_data : transparent_chunks)
    {
        glm::mat4 model = getChunkModelMatrix(chunk_data.position);
        glUniformMatrix4fv(uniform_model, 1, GL_FALSE, glm::value_ptr(model));
        chunk_data.chunk->mesh->render();
        total_triangles_rendered += chunk_data.chunk->mesh->index_count / 3;
    }

    // ========== RESET OPENGL STATE ==========
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Calculate frame time
    auto frame_end = std::chrono::high_resolution_clock::now();
    last_frame_time = std::chrono::duration<float, std::milli>(frame_end - frame_start).count();

    // Log rendering performance periodically
    static int render_debug_counter = 0;
    static float total_render_time = 0.0f;
    static int render_samples = 0;

    total_render_time += last_frame_time;
    render_samples++;

    if (++render_debug_counter % 300 == 0) // Every 5 seconds at 60fps
    {
        float avg_render_time = total_render_time / render_samples;
        std::cout << "RENDER PERFORMANCE SUMMARY:" << std::endl;
        std::cout << "  Average render time: " << avg_render_time << "ms" << std::endl;
        std::cout << "  Chunks rendered: " << chunks_rendered_last_frame << std::endl;
        std::cout << "  Vertices rendered: " << vertices_rendered_last_frame << std::endl;
        std::cout << "  Triangles rendered: " << total_triangles_rendered << std::endl;

        // Reset counters
        total_render_time = 0.0f;
        render_samples = 0;
    }
}

VoxelID VoxelRenderer::getVoxel(int x, int y, int z) const
{
    if (!world)
    {
        return VOXEL_AIR;
    }
    return world->getVoxel(x, y, z);
}

void VoxelRenderer::setVoxel(int x, int y, int z, VoxelID voxel)
{
    if (!world)
    {
        return;
    }
    world->setVoxel(x, y, z, voxel);
}

size_t VoxelRenderer::getLoadedChunkCount() const
{
    return world ? world->getLoadedChunkCount() : 0;
}

void VoxelRenderer::setRenderDistance(int distance)
{
    if (world)
    {
        world->setRenderDistance(distance);
    }
}

int VoxelRenderer::getRenderDistance() const
{
    return world ? world->getRenderDistance() : 0;
}

bool VoxelRenderer::loadShaders()
{
    try
    {
        shader = std::make_unique<Shader>("shaders/voxel.vs", "shaders/voxel.fs");
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Shader loading error: " << e.what() << std::endl;

        // Try alternative paths
        try
        {
            shader = std::make_unique<Shader>("voxel world/voxel.vs", "voxel world/voxel.fs");
            return true;
        }
        catch (const std::exception &e2)
        {
            std::cerr << "Alternative shader loading error: " << e2.what() << std::endl;
            return false;
        }
    }
}

bool VoxelRenderer::loadTextures()
{
    // Define the texture files we need in order
    std::vector<std::string> texture_files = {
        "air.png",              // 0 - placeholder (not used)
        "stone.png",            // 1 - Stone
        "dirt.png",             // 2 - Dirt
        "grass_block_top.png",  // 3 - Grass top
        "grass_block_side.png", // 4 - Grass side
        "cobblestone.png",      // 5 - Cobblestone
        "spruce_log_top.png",   // 6 - Wood log top
        "spruce_log.png",       // 7 - Wood log side
        "spruce_leaves.png",    // 8 - Leaves
        "sand.png",             // 9 - Sand
        "water_still.png",      // 10 - 41 - Water frames (will be extracted)
        "glass.png",            // 42 - Glass
        "iron_block.png"        // 43 - Iron
    };

    const int texture_size = 16; // Each Minecraft texture is 16x16
    const int atlas_width = 9;   // 8 textures per row (increased for water frames)
    const int atlas_height = 5;  // 5 rows
    const int atlas_pixel_width = atlas_width * texture_size;
    const int atlas_pixel_height = atlas_height * texture_size;

    // Create atlas data
    std::vector<unsigned char> atlas_data(((atlas_pixel_width * atlas_pixel_height) - 2) * 4, 255); // RGBA

    // Base path to your textures
    std::string texture_base_path = "voxel world/Textures/";

    int current_atlas_index = 0;

    // Load each texture into atlas
    for (size_t i = 0; i < texture_files.size(); i++)
    {
        std::string texture_path = texture_base_path + texture_files[i];

        int width, height, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char *image_data = stbi_load(texture_path.c_str(), &width, &height, &channels, 4); // Force RGBA

        if (!image_data)
        {
            std::cerr << "Failed to load texture: " << texture_path << std::endl;
            // Create a fallback colored texture
            int atlas_x = current_atlas_index % atlas_width;
            int atlas_y = current_atlas_index / atlas_width;

            for (int y = 0; y < texture_size; y++)
            {
                for (int x = 0; x < texture_size; x++)
                {
                    int atlas_pixel_x = atlas_x * texture_size + x;
                    int atlas_pixel_y = atlas_y * texture_size + y;
                    int atlas_index = (atlas_pixel_y * atlas_pixel_width + atlas_pixel_x) * 4;

                    // Create different colored fallback for each texture
                    atlas_data[atlas_index + 0] = (i * 50) % 255;  // R
                    atlas_data[atlas_index + 1] = (i * 80) % 255;  // G
                    atlas_data[atlas_index + 2] = (i * 120) % 255; // B
                    atlas_data[atlas_index + 3] = 255;             // A
                }
            }
            current_atlas_index++;
            continue;
        }

        // Special handling for water texture (animated strip)
        if (texture_files[i] == "water_still.png")
        {
            // Water texture is typically a vertical strip of frames
            int frame_count = height / width; // Assume square frames
            std::cout << "Water texture has " << frame_count << " frames" << std::endl;

            // Extract each frame
            for (int frame = 0; frame < frame_count && current_atlas_index < atlas_width * atlas_height; frame++)
            {
                int atlas_x = current_atlas_index % atlas_width;
                int atlas_y = current_atlas_index / atlas_width;

                // Extract frame from the strip
                for (int y = 0; y < texture_size && y < width; y++)
                {
                    for (int x = 0; x < texture_size && x < width; x++)
                    {
                        int src_index = ((frame * width + y) * width + x) * 4;
                        int atlas_pixel_x = atlas_x * texture_size + x;
                        int atlas_pixel_y = atlas_y * texture_size + y;
                        int atlas_index = (atlas_pixel_y * atlas_pixel_width + atlas_pixel_x) * 4;

                        atlas_data[atlas_index + 0] = image_data[src_index + 0]; // R
                        atlas_data[atlas_index + 1] = image_data[src_index + 1]; // G
                        atlas_data[atlas_index + 2] = image_data[src_index + 2]; // B
                        atlas_data[atlas_index + 3] = image_data[src_index + 3]; // A
                    }
                }

                std::cout << "Loaded water frame " << frame << " to atlas index " << current_atlas_index << std::endl;
                current_atlas_index++;
            }

            // Store water animation info
            water_frame_start = 10; // First water frame index
            water_frame_count = frame_count;
        }
        else
        {
            // Regular texture loading
            int atlas_x = current_atlas_index % atlas_width;
            int atlas_y = current_atlas_index / atlas_width;

            for (int y = 0; y < texture_size && y < height; y++)
            {
                for (int x = 0; x < texture_size && x < width; x++)
                {
                    int src_index = (y * width + x) * 4;
                    int atlas_pixel_x = atlas_x * texture_size + x;
                    int atlas_pixel_y = atlas_y * texture_size + y;
                    int atlas_index = (atlas_pixel_y * atlas_pixel_width + atlas_pixel_x) * 4;

                    atlas_data[atlas_index + 0] = image_data[src_index + 0]; // R
                    atlas_data[atlas_index + 1] = image_data[src_index + 1]; // G
                    atlas_data[atlas_index + 2] = image_data[src_index + 2]; // B
                    atlas_data[atlas_index + 3] = image_data[src_index + 3]; // A
                }
            }

            std::cout << "Loaded texture: " << texture_files[i] << " to atlas index " << current_atlas_index << std::endl;
            current_atlas_index++;
        }

        stbi_image_free(image_data);
    }

    // Create OpenGL texture
    glGenTextures(1, &block_texture_atlas);
    glBindTexture(GL_TEXTURE_2D, block_texture_atlas);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_pixel_width, atlas_pixel_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas_data.data());

    // Set texture parameters for pixel art
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "Texture atlas created successfully!" << std::endl;
    return true;
}

void VoxelRenderer::setupShaderUniforms()
{
    if (!shader)
    {
        return;
    }

    shader->use();

    // Get uniform locations
    uniform_model = glGetUniformLocation(shader->ID, "model");
    uniform_view = glGetUniformLocation(shader->ID, "view");
    uniform_projection = glGetUniformLocation(shader->ID, "projection");
    uniform_texture_atlas = glGetUniformLocation(shader->ID, "texture_atlas");
    uniform_time = glGetUniformLocation(shader->ID, "time");
    uniform_render_pass = glGetUniformLocation(shader->ID, "renderPass"); // ADD THIS

    if (uniform_model == -1)
        std::cerr << "Warning: 'model' uniform not found in shader" << std::endl;
    if (uniform_view == -1)
        std::cerr << "Warning: 'view' uniform not found in shader" << std::endl;
    if (uniform_projection == -1)
        std::cerr << "Warning: 'projection' uniform not found in shader" << std::endl;
    if (uniform_texture_atlas == -1)
        std::cerr << "Warning: 'texture_atlas' uniform not found in shader" << std::endl;
    if (uniform_time == -1)
        std::cerr << "Warning: 'time' uniform not found in shader" << std::endl;
    if (uniform_render_pass == -1)
        std::cerr << "Warning: 'renderPass' uniform not found in shader" << std::endl;
}

void VoxelRenderer::renderChunk(const VoxelChunk &chunk, const glm::mat4 &model_matrix)
{
    if (!chunk.mesh || !chunk.mesh->isUploaded())
    {
        return;
    }

    // Set model matrix
    glUniformMatrix4fv(uniform_model, 1, GL_FALSE, glm::value_ptr(model_matrix));

    // Render the mesh
    chunk.mesh->render();

    // Update statistics
    chunks_rendered_last_frame++;
    vertices_rendered_last_frame += chunk.mesh->vertex_count;
}

glm::mat4 VoxelRenderer::getChunkModelMatrix(const glm::ivec3 &chunk_pos) const
{
    glm::vec3 world_pos = glm::vec3(
        chunk_pos.x * CHUNK_SIZE,
        chunk_pos.y * CHUNK_HEIGHT,
        chunk_pos.z * CHUNK_SIZE);

    return glm::translate(glm::mat4(1.0f), world_pos);
}

int VoxelRenderer::getCurrentWaterTextureIndex() const
{
    if (water_frame_count == 0)
        return 10; // Fallback to static texture

    // Animate at 8 * 4 FPS (change frame every 0.125 * 4 seconds)
    float frame_time = 0.125f * 4;
    int current_frame = (int)(water_animation_time / frame_time) % water_frame_count;
    return water_frame_start + current_frame;
}

bool VoxelRenderer::isChunkInFrustum(const glm::ivec3 &chunk_pos, const Camera &camera, const glm::mat4 &projection) const
{
    glm::vec3 chunk_world_pos = glm::vec3(
        chunk_pos.x * CHUNK_SIZE + CHUNK_SIZE * 0.5f,
        chunk_pos.y * CHUNK_HEIGHT + CHUNK_HEIGHT * 0.5f,
        chunk_pos.z * CHUNK_SIZE + CHUNK_SIZE * 0.5f);

    float distance = glm::distance(camera.Position, chunk_world_pos);

    // Fix: Use chunk distance, not block distance
    float chunk_distance = distance / CHUNK_SIZE;
    float max_render_distance_chunks = (float)getRenderDistance() * 1.2f; // 20% margin

    return chunk_distance <= max_render_distance_chunks;
}

int VoxelRenderer::getChunkLOD(const glm::ivec3 &chunk_pos, const Camera &camera) const
{
    glm::vec3 chunk_world_pos = glm::vec3(
        chunk_pos.x * CHUNK_SIZE + CHUNK_SIZE * 0.5f,
        chunk_pos.y * CHUNK_HEIGHT + CHUNK_HEIGHT * 0.5f,
        chunk_pos.z * CHUNK_SIZE + CHUNK_SIZE * 0.5f);

    float distance = glm::distance(camera.Position, chunk_world_pos);
    float chunk_size_f = (float)CHUNK_SIZE;

    if (distance < chunk_size_f * 4.0f)
        return 0; // Full detail
    else if (distance < chunk_size_f * 8.0f)
        return 1; // Half detail
    else
        return 2; // Quarter detail
}
