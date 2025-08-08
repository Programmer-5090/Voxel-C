#include <glad/glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "voxel world/voxel_renderer.h"
#include "heightmap_generator.h"

#include <iostream>
#include <sstream>
#include <array>
#include <memory>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;

// camera
Camera camera(glm::vec3(0.0f, 80.0f, 10.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f; // time between current frame and last frame
float lastFrame = 0.0f;

// face culling toggle
bool faceCullingEnabled = true;
bool faceCullingKeyPressed = false;

// wireframe mode toggle
bool wireframeEnabled = false;
bool wireframeKeyPressed = false;

// voxel interaction
bool leftMousePressed = false;
bool rightMousePressed = false;

// Voxel renderer
std::unique_ptr<VoxelRenderer> voxelRenderer;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Voxel World - OpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // Enable face culling for better performance
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK); // Cull back faces (faces facing away from camera)
    glFrontFace(GL_CCW); // Counter-clockwise vertices define front faces

    // Print controls
    std::cout << "=== Voxel World Controls ===" << std::endl;
    std::cout << "WASD: Move camera" << std::endl;
    std::cout << "Mouse: Look around" << std::endl;
    std::cout << "Left Click: Remove voxel" << std::endl;
    std::cout << "Right Click: Place stone voxel" << std::endl;
    std::cout << "F: Toggle face culling" << std::endl;
    std::cout << "G: Toggle wireframe mode" << std::endl;
    std::cout << "R: Print camera position" << std::endl;
    std::cout << "ESC: Exit" << std::endl;
    std::cout << "=============================" << std::endl;

    // Initialize voxel renderer
    voxelRenderer = std::make_unique<VoxelRenderer>(12345); // Using seed 12345

    if (!voxelRenderer->initialize())
    {
        std::cout << "Failed to initialize voxel renderer!" << std::endl;
        return -1;
    }

    // Generate heightmaps for analysis (optional - comment out if not needed)
    std::cout << "Generating heightmaps..." << std::endl;
    // HeightmapGenerator::generateAllHeightmaps(12345, 512, 512);
    std::cout << "Heightmaps saved to 'heightmaps/' directory!" << std::endl;

    std::cout << "Voxel world initialized successfully!" << std::endl;
    std::cout << "Starting position: " << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << std::endl;

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // Update voxel world
        if (voxelRenderer)
        {
            voxelRenderer->update(camera);
        }

        // render
        // ------
        // glClearColor(0.5f, 0.8f, 1.0f, 1.0f); // Sky blue background
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render voxel world
        if (voxelRenderer)
        {
            glm::mat4 projection = glm::perspective(
                glm::radians(camera.Zoom),
                (float)SCR_WIDTH / (float)SCR_HEIGHT,
                0.1f, 1000.0f);

            voxelRenderer->render(camera, projection);
        }

        // glfw: swap buffers and poll IO events
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    voxelRenderer.reset();

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);

    // Toggle face culling with F key
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !faceCullingKeyPressed)
    {
        faceCullingEnabled = !faceCullingEnabled;
        if (faceCullingEnabled)
        {
            glEnable(GL_CULL_FACE);
            std::cout << "Face culling enabled" << std::endl;
        }
        else
        {
            glDisable(GL_CULL_FACE);
            std::cout << "Face culling disabled" << std::endl;
        }
        faceCullingKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE)
    {
        faceCullingKeyPressed = false;
    }

    // Toggle wireframe mode with G key
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS && !wireframeKeyPressed)
    {
        wireframeEnabled = !wireframeEnabled;
        if (wireframeEnabled)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            std::cout << "Wireframe mode enabled" << std::endl;
        }
        else
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            std::cout << "Wireframe mode disabled" << std::endl;
        }
        wireframeKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_RELEASE)
    {
        wireframeKeyPressed = false;
    }

    // Print camera position with R key
    static bool rKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !rKeyPressed)
    {
        std::cout << "Camera position: " << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << std::endl;
        if (voxelRenderer)
        {
            std::cout << "Chunks rendered: " << voxelRenderer->getChunksRendered() << std::endl;
        }
        rKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE)
    {
        rKeyPressed = false;
    }

    // Voxel placement/removal with mouse clicks
    bool leftMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (leftMouse && !leftMousePressed && voxelRenderer)
    {
        // Raycast from camera to find voxel to remove
        glm::vec3 rayStart = camera.Position;
        glm::vec3 rayDir = camera.Front;
        glm::ivec3 lastAirPos;
        bool blockFound = false;

        for (float t = 0.0f; t < 10.0f; t += 0.05f)
        {
            glm::vec3 rayPos = rayStart + rayDir * t;
            glm::ivec3 blockPos = glm::ivec3(floor(rayPos.x), floor(rayPos.y), floor(rayPos.z));

            VoxelID voxel = voxelRenderer->getVoxel(blockPos.x, blockPos.y, blockPos.z);

            if (voxel != VOXEL_AIR)
            {
                voxelRenderer->setVoxel(blockPos.x, blockPos.y, blockPos.z, VOXEL_AIR);
                std::cout << "Removed voxel at (" << blockPos.x
                          << ", " << blockPos.y
                          << ", " << blockPos.z << ")" << std::endl;
                blockFound = true;
                break;
            }
            lastAirPos = blockPos;
        }
    }

    if (rightMouse && !rightMousePressed && voxelRenderer)
    {
        // Raycast to find where to place voxel
        glm::vec3 rayStart = camera.Position;
        glm::vec3 rayDir = camera.Front;
        glm::ivec3 lastAirPos;
        bool blockFound = false;

        for (float t = 0.0f; t < 10.0f; t += 0.05f)
        {
            glm::vec3 rayPos = rayStart + rayDir * t;
            glm::ivec3 blockPos = glm::ivec3(floor(rayPos.x), floor(rayPos.y), floor(rayPos.z));

            VoxelID voxel = voxelRenderer->getVoxel(blockPos.x, blockPos.y, blockPos.z);

            if (voxel != VOXEL_AIR)
            {
                voxelRenderer->setVoxel(lastAirPos.x, lastAirPos.y, lastAirPos.z, VOXEL_STONE);
                std::cout << "Placed stone voxel at (" << lastAirPos.x
                          << ", " << lastAirPos.y
                          << ", " << lastAirPos.z << ")" << std::endl;
                blockFound = true;
                break;
            }
            lastAirPos = blockPos;
        }
    }

    leftMousePressed = leftMouse;
    rightMousePressed = rightMouse;
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
