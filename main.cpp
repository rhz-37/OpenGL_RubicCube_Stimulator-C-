/*
  example_main.cpp
  Minimal example showing how to draw the 54 sticker quads produced by Core
  (core.h / core.cpp from previous message) using GLFW + GLAD + GLM + OpenGL.

  Build (example, adjust include/library paths for your system):
    g++ example_main.cpp core.cpp -I/path/to/glm -I/path/to/glad/include -lglfw -ldl -lGL -o rubik

  Notes:
  - core.h/core.cpp must be in same directory (or adjust includes).
  - This example uses simple per-sticker draw calls (no instancing) for clarity.
  - Press keys U D L R F B to queue face turns.
    Hold SHIFT to make the move a prime (counter-clockwise). Hold CTRL to make it a double (2).
*/

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <chrono>

#include "core.h"

// Simple shader sources embedded here for convenience
static const char* vertexShaderSrc = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 uColor; // passed through for flat shading

out vec3 vColor;

void main() {
    vColor = uColor;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)glsl";

static const char* fragmentShaderSrc = R"glsl(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)glsl";

// helper: compile shader program
GLuint compileProgram(const char* vsSrc, const char* fsSrc) {
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            glGetShaderInfoLog(s, len, nullptr, &log[0]);
            std::cerr << "Shader compile error: " << log << std::endl;
            glDeleteShader(s);
            return 0;
        }
        return s;
        };

    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, &log[0]);
        std::cerr << "Program link error: " << log << std::endl;
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// create a simple quad VAO (two triangles) in XY plane centered at origin covering [-0.5..0.5]
GLuint createQuadVAO() {
    float verts[] = {
        // pos.x, pos.y, pos.z
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,

        -0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return vao;
}

// Key callback to map UDLRFB keys to cube moves and queue them into Core stored in window user pointer.
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    Core* core = reinterpret_cast<Core*>(glfwGetWindowUserPointer(window));
    if (!core) return;

    std::string move;
    bool prime = (mods & GLFW_MOD_SHIFT) != 0;
    bool dbl = (mods & GLFW_MOD_CONTROL) != 0;

    auto push = [&](char face) {
        move.clear();
        move.push_back(face);
        if (dbl) move.push_back('2');
        if (prime && !dbl) move.push_back('\''); // "R2'" is unusual; we only do R' when shift + not ctrl
        core->queueMove(move);
        std::cout << "Queued move: " << move << std::endl;
        };

    switch (key) {
    case GLFW_KEY_U: push('U'); break;
    case GLFW_KEY_D: push('D'); break;
    case GLFW_KEY_L: push('L'); break;
    case GLFW_KEY_R: push('R'); break;
    case GLFW_KEY_F: push('F'); break;
    case GLFW_KEY_B: push('B'); break;
    case GLFW_KEY_SPACE: core->clearQueue(); break;
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
    default: break;
    }
}

int main() {
    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Rubik Core - Sticker Quad Example", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return 1;
    }

    glEnable(GL_DEPTH_TEST);

    GLuint program = compileProgram(vertexShaderSrc, fragmentShaderSrc);
    if (!program) return 1;
    GLuint vao = createQuadVAO();

    // create Core simulation instance and attach to window for callbacks
    Core core(0.9f /*cubieSize*/, 0.03f /*gap*/, 720.0f /*deg/sec, fast*/);
    glfwSetWindowUserPointer(window, &core);
    glfwSetKeyCallback(window, keyCallback);

    // uniform locations
    GLint locModel = glGetUniformLocation(program, "model");
    GLint locView = glGetUniformLocation(program, "view");
    GLint locProj = glGetUniformLocation(program, "projection");
    GLint locColor = glGetUniformLocation(program, "uColor");

    // camera setup
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
    glm::vec3 camPos(4.0f, 4.0f, 6.0f);
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0, 1, 0));

    // time tracking
    double lastTime = glfwGetTime();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;

        // update simulation
        core.update(dt);

        // fetch sticker transforms & colors
        std::vector<glm::mat4> mats = core.getStickerModelMatrices();
        std::vector<glm::vec3> cols = core.getStickerColors();
        if (mats.size() != cols.size()) {
            std::cerr << "Core returned mismatched arrays\n";
            break;
        }

        // render
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        projection = glm::perspective(glm::radians(45.0f), width / float(height), 0.1f, 100.0f);

        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);
        glUniformMatrix4fv(locView, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(locProj, 1, GL_FALSE, &projection[0][0]);

        glBindVertexArray(vao);
        // Draw each sticker: set model and color and draw the quad
        for (size_t i = 0; i < mats.size(); ++i) {
            glUniformMatrix4fv(locModel, 1, GL_FALSE, &mats[i][0][0]);
            glm::vec3 c = cols[i];
            glUniform3f(locColor, c.r, c.g, c.b);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glBindVertexArray(0);
        glUseProgram(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    glfwTerminate();
    return 0;
}