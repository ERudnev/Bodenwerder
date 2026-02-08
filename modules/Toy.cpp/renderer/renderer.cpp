#include "renderer.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>

namespace {
  // Vertex shader source
  const char* kVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
)";

  // Fragment shader source
  const char* kFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f); // Orange color
}
)";

  bool compile_shader(GLuint shader, const char* label) {
    glCompileShader(shader);
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success) return true;

    char infoLog[512];
    glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    std::cerr << label << " compilation failed: " << infoLog << std::endl;
    return false;
  }

  bool link_program(GLuint program) {
    glLinkProgram(program);
    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success) return true;

    char infoLog[512];
    glGetProgramInfoLog(program, 512, nullptr, infoLog);
    std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    return false;
  }
}

namespace Toy {

int run_render_demo() {
  // Initialize GLFW
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return 1;
  }

  // Set OpenGL version to 3.3
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Create window
  GLFWwindow* window =
      glfwCreateWindow(800, 600, "OpenGL Hello World Triangle", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return 2;
  }

  glfwMakeContextCurrent(window);

  // Initialize GLEW
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
    glfwTerminate();
    return 3;
  }

  // Print OpenGL version
  std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "Press ESC to exit." << std::endl;

  // Set viewport
  glfwSetFramebufferSizeCallback(
      window, [](GLFWwindow*, int width, int height) {
        glViewport(0, 0, width, height);
      });

  // Triangle vertices
  float vertices[] = {
      -0.5f, -0.5f, 0.0f,  // Bottom left
      0.5f,  -0.5f, 0.0f,  // Bottom right
      0.0f,  0.5f,  0.0f   // Top
  };

  // Create VBO and VAO
  GLuint VBO = 0, VAO = 0;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                        (void*)0);
  glEnableVertexAttribArray(0);

  // Create vertex shader
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &kVertexShaderSource, nullptr);
  if (!compile_shader(vertexShader, "Vertex shader")) {
    glfwTerminate();
    return 4;
  }

  // Create fragment shader
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &kFragmentShaderSource, nullptr);
  if (!compile_shader(fragmentShader, "Fragment shader")) {
    glfwTerminate();
    return 5;
  }

  // Create shader program
  GLuint shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  if (!link_program(shaderProgram)) {
    glfwTerminate();
    return 6;
  }

  // Clean up shaders
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  // Set clear color
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Process input
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
    }

    // Clear screen
    glClear(GL_COLOR_BUFFER_BIT);

    // Use shader program and draw triangle
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Swap buffers and poll events
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Clean up
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shaderProgram);

  glfwTerminate();
  return 0;
}

}  // namespace Toy


