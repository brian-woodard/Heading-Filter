
#include <stdio.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define WIDTH  800
#define HEIGHT 800

// NOTE: Uncomment the following line for GL error handling
//#define GL_DEBUG

#ifdef GL_DEBUG
#define GLCALL(function) \
   { \
      GLenum error = GL_INVALID_ENUM; \
      while (error != GL_NO_ERROR) \
      { \
         error = glGetError(); \
      } \
      function; \
      error = glGetError(); \
      if (error != GL_NO_ERROR) \
      { \
         fprintf(stderr, "OpenGL Error: GL_ENUM(%d) at %s:%d\n", error, __FILE__, __LINE__); \
      } \
   }
#else
#define GLCALL(function) function;
#endif

float normalize(float Input)
{
   return Input - 360.0f * std::floor((Input + 180.0f)/(360.0f));
}

int main(int argc, char* argv[])
{
   GLFWwindow* window = nullptr;

   // initialize glfw
   if (!glfwInit())
      return 0;

   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

   // Create window
   window = glfwCreateWindow(WIDTH, HEIGHT, "Heading Filter", NULL, NULL);

   if (!window)
   {
      glfwTerminate();
      return 0;
   }

   // make the window's context current
   glfwMakeContextCurrent(window);

   // use glad to load OpenGL function pointers
   if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
   {
      printf("Error: Failed to initialize GLAD.\n");
      glfwTerminate();
      return 0;
   }

   glfwSetWindowSize(window, WIDTH, HEIGHT);

   // Setup Dear ImGui
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGui::StyleColorsDark();

   // Setup Platform/Render backends
   ImGui_ImplGlfw_InitForOpenGL(window, true);
   ImGui_ImplOpenGL3_Init("#version 330");

   // Make the window visible
   glfwShowWindow(window);

   // Initialize opengl
   GLCALL(glClearColor(0.5, 0.5, 0.5, 1.0));

   // enable blending
   GLCALL(glEnable(GL_BLEND));
   GLCALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

   // set frame rate to 60 Hz
   using framerate = std::chrono::duration<double, std::ratio<1, 60>>;
   auto frame_time = std::chrono::high_resolution_clock::now() + framerate{1};

   float time_constant = 0.5;
   float lag_filter_c1 = exp(-0.016666 / time_constant);
   float lag_filter_c2 = 1.0 - lag_filter_c1;
   float heading_input = 0.0;
   float heading_input_norm_0_360 = 0.0;
   float heading_input_norm_0_360_prev = 0.0;
   float heading_cmd = 0.0;
   float heading_cmd_prev = 0.0;
   float heading_filtered = 0.0;
   float heading_filtered_prev = 0.0;
   float heading_delta = 0.0;
   float heading_output = 0.0;
   float deltas[300] = {};
   int   offset = 0;
   bool  use_orig_filter = true;

   while (window)
   {
      // Poll events
      glfwPollEvents();

      if (glfwWindowShouldClose(window))
      {
         glfwTerminate();
         window = nullptr;
         break;
      }

      GLCALL(glClear(GL_COLOR_BUFFER_BIT));

      // Start the Dear ImGui frame
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      ImGui::Begin("Heading Filter");

      if (ImGui::SliderFloat("Time Constant", &time_constant, 0.0, 10.0, "%.6f"))
      {
         lag_filter_c1 = exp(-0.016666 / time_constant);
         lag_filter_c2 = 1.0 - lag_filter_c1;
      }
      ImGui::Text("Lag Filter C1: %f", lag_filter_c1);
      ImGui::Text("Lag Filter C2: %f", lag_filter_c2);
      ImGui::Checkbox("Original Filter", &use_orig_filter);
      ImGui::SliderFloat("Heading Input", &heading_input, -540.0, 540.0, "%.6f");

      // Handle boundary condition
      heading_input_norm_0_360 = heading_input;
      if (heading_input_norm_0_360 > 360.0)
      {
         heading_input_norm_0_360 -= 360.0;
      }
      else if (heading_input_norm_0_360 < 0.0)
      {
         heading_input_norm_0_360 += 360.0;
      }

      ImGui::Separator();
      ImGui::SliderFloat("Heading Input Normalized", &heading_input_norm_0_360, -540.0, 540.0, "%.6f");
      ImGui::SliderFloat("Heading Command", &heading_cmd, -540.0, 540.0, "%.6f");
      ImGui::SliderFloat("Heading Filtered", &heading_filtered, -540.0, 540.0, "%.6f");
      ImGui::SliderFloat("Heading Output", &heading_output, -540.0, 540.0, "%.6f");

      heading_cmd = heading_input_norm_0_360;

      if (use_orig_filter)
      {
         // Lag filter with boundary issues
         heading_filtered = lag_filter_c1 * heading_filtered + lag_filter_c2 * heading_cmd;
         heading_delta = heading_filtered - heading_filtered_prev;
      }
      else
      {
         // Calculate shortest angular distance (-180 to 180)
         heading_delta = fmodf((heading_cmd - heading_output + 180.0f), 360.0f) - 180.0f;

         heading_filtered = fmodf((heading_filtered + lag_filter_c2 * heading_delta), 360.0f);
      }

      heading_output = normalize(heading_filtered);

      heading_filtered_prev = heading_filtered;

      deltas[offset] = heading_delta;
      offset = (offset + 1) % IM_ARRAYSIZE(deltas);
      ImGui::Text("Delta: %f", heading_delta);
      ImGui::PlotLines("Delta", deltas, IM_ARRAYSIZE(deltas), offset, nullptr, -180.0f, 180.0f, ImVec2(0, 80.0f));

      ImGui::End();

      // Render ImGui
      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);

      // wait until next frame
      std::this_thread::sleep_until(frame_time);
      frame_time += framerate{1};
   }

   return 0;
}
