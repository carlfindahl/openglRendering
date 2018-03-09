#include "glfwApplication.h"
#include "gl_cpp.hpp"
#include "GLFW/glfw3.h"
#include "vertexArray.h"	
#include "serviceLocator.h"
#include "uniformBlocks.h"
#include "logging.h"
#include "vertex.h"
#include "buffer.h"
#include "shader.h"
#include "linalg.h"
#include "camera.h"
#include "texture.h"
#include "randomEngine.h"
#include "textureView.h"
#include "image.h"
#include "files.h"
#include "clock.h"
#include "shapes.h"

#include "imgui.h"
#include "imgui_glfw.h"

#include <array>
#include "renderBatch.h"
#include "transformation.h"

// ImGui extern for input management
extern bool g_MouseJustPressed[3];

// GLFW Key Callback
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    static InputManager* inputManager = ServiceLocator<InputManager>::get();

    ImGuiIO& io = ImGui::GetIO();
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        if (!io.WantCaptureKeyboard) // If ImGui is not reserving input
            inputManager->pressKey(key);
        io.KeysDown[key] = true; // ImGui
        logCustom()->debug("Pressed key: {}", key);
    }
    else if (action == GLFW_RELEASE)
    {
        inputManager->releaseKey(key);
        io.KeysDown[key] = false; // ImGui
        logCustom()->debug("Released key: {}", key);
    }

    (void)mods; // Modifiers are not reliable across systems
    io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
}

// GLFW Mouse Button Callback
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    static InputManager* inputManager = ServiceLocator<InputManager>::get();
    ImGuiIO& io = ImGui::GetIO();

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        if (!io.WantCaptureMouse) // If ImGui is not reserving input
            inputManager->pressKey(button);
        logCustom()->debug("Pressed mouse button: {}", button);

        // ImGui Interaction
        if (button >= 0 && button < 3)
            g_MouseJustPressed[button] = true;
    }
    else if (action == GLFW_RELEASE)
    {
        inputManager->releaseKey(button);
        logCustom()->debug("Released mouse button: {}", button);
    }
}

// GLFW Cursor Position Callback
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    static InputManager* inputManager = ServiceLocator<InputManager>::get();
    inputManager->moveCursor({ xpos, ypos });
    logCustom()->debug("Cursor at: X[{}] Y[{}]", xpos, ypos);
}

// GLFW Mouse Scroll Wheel Callback
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    static InputManager* inputManager = ServiceLocator<InputManager>::get();
    inputManager->scrollMouse({ xoffset, yoffset });
    logCustom()->debug("Scrolldelta: X[{}] Y[{}]", xoffset, yoffset);

    // ImGui Interaction
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheel += (float)yoffset;
}

// GLFW Error Callback
static void error_callback(int error, const char* description)
{
    logCustom()->error("GLFW Error #{}: {}", error, description);
}

GLFWApplication::GLFWApplication()
{
    ServiceLocator<InputManager>::provide(&mInputManager);
    glfwInit();

    // Context Hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 2); // 2x MSAA

    // Context Creation [Windowed Full screen Mode]
    auto monitor = glfwGetPrimaryMonitor();
    auto mode = glfwGetVideoMode(monitor);
    auto monitorName = glfwGetMonitorName(monitor);

    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    logCustom()->info("Starting on {} in {}x{}", monitorName, mode->width, mode->height);

    mWindow = glfwCreateWindow(mode->width, mode->height, "Open GL Rendering", monitor, nullptr);
    glfwMakeContextCurrent(mWindow);
    glfwSwapInterval(1);

    // GLFW Callbacks
    glfwSetErrorCallback(error_callback);
    glfwSetKeyCallback(mWindow, key_callback);
    glfwSetMouseButtonCallback(mWindow, mouse_button_callback);
    glfwSetCursorPosCallback(mWindow, cursor_position_callback);
    glfwSetScrollCallback(mWindow, scroll_callback);

    // GL Setup
    gl::Enable(gl::DEPTH_TEST);
    gl::DepthFunc(gl::LEQUAL);

    // ImGui Setup
    mImGuiContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplGlfwGL3_Init(mWindow);
    ImGui::StyleColorsDark();

    auto fontPath = getResourcePath("ProggyTiny.ttf");
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 10.f);
}

GLFWApplication::~GLFWApplication()
{
    ImGui_ImplGlfwGL3_Shutdown();
    ImGui::DestroyContext(mImGuiContext);
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

void GLFWApplication::run()
{
    Shader basicShader(getResourcePath("vertex.vs"), getResourcePath("frag.fs"));
    basicShader.bind();

    UMatrices matrixUniforms;
    matrixUniforms.modelView = mat4::translate(0.f, -32.f, -1.5f);
    matrixUniforms.projection = mat4::perspective(59.f, 1280.f / 720.f, 0.01f, 256.f);

    UniformBuffer matrixBuffer(sizeof(UMatrices), basicShader);
    matrixBuffer.setUniformBlock("Matrices");
    matrixBuffer.setBlockData(&matrixUniforms, sizeof(UMatrices));
    matrixBuffer.bind(1);

    // Objects
    Quad grassBlade{ vec2(2.f, 4.f), vec3(0.05f, 0.86f, 0.33f) };
    
    Texture scatterMap{ getResourcePath("noisemap.png") };
    scatterMap.setRepeatMode(ETextureRepeatMode::MirrorRepeat);
    scatterMap.bind(0);

    Texture highRise{ getResourcePath("highrise.png") };
    highRise.setRepeatMode(ETextureRepeatMode::MirrorRepeat);
    highRise.bind(1);

    gl::ClearColor(0.1f, 0.f, 0.05f, 1.f);

    // Renderer
    VertexArray batchVao;
    batchVao.addAttribute(3, gl::FLOAT, offsetof(Vertex, x));
    batchVao.addAttribute(3, gl::FLOAT, offsetof(Vertex, r));
    batchVao.addAttribute(2, gl::FLOAT, offsetof(Vertex, u));

    RenderBatch shapeBatch(std::move(batchVao));
    shapeBatch.clear();
    shapeBatch.push(grassBlade);
    shapeBatch.commit();

    shapeBatch.bind();

    // Delta Time Measurement
    auto deltaClock = Clock{};
    auto updateDelta = Clock::TimeUnit{ 1.f / 144.f };
    auto timeSinceUpdate = Clock::TimeUnit{ 0.f };

    int instanceCount = 32768;
    while (!glfwWindowShouldClose(mWindow))
    {
        // Clock Update
        const auto deltaTime = deltaClock.restart().count();
        timeSinceUpdate += Clock::TimeUnit{ deltaTime };

        // Event Processing
        mInputManager.clear();
        glfwPollEvents();
        ImGui_ImplGlfwGL3_NewFrame();

        // Input Handling
        if (mInputManager.wasPressed(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(mWindow, true);
        if (mInputManager.arePressed(GLFW_KEY_D))
            matrixUniforms.modelView(0, 3) -= 25.f * deltaTime;
        if (mInputManager.arePressed(GLFW_KEY_A))
            matrixUniforms.modelView(0, 3) += 25.f * deltaTime;
        if (mInputManager.arePressed(GLFW_KEY_W))
            matrixUniforms.modelView(1, 3) -= 25.f * deltaTime;
        if (mInputManager.arePressed(GLFW_KEY_S))
            matrixUniforms.modelView(1, 3) += 25.f * deltaTime;
        if (mInputManager.arePressed(GLFW_KEY_E))
            matrixUniforms.modelView *= mat4::rotate(10.f * deltaTime, 1.f, 0.f, 0.f);
        if (mInputManager.arePressed(GLFW_KEY_Q))
            matrixUniforms.modelView *= mat4::rotate(-10.f * deltaTime, 1.f, 0.f, 0.f);
        
        matrixBuffer.setPartialBlockData("modelView", matrixUniforms.modelView.data(), 64);

        ImGui::Text("FPS: %.2f", 1.f / deltaTime);
        ImGui::DragInt("Instances", &instanceCount, 4.f, 1, 32768);

        // Updating
        while (timeSinceUpdate > updateDelta)
        {
            timeSinceUpdate -= updateDelta;
        }

        // Drawing
        gl::Clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);
        gl::DrawElementsInstancedBaseVertex(gl::TRIANGLES, shapeBatch.getIndexCount(), gl::UNSIGNED_INT, nullptr, instanceCount, 0);

        ImGui::Render();
        ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(mWindow);
    }
}
