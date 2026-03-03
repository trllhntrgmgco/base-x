#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

static void drawCircle(float cx, float cy, float r, int segments) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * 3.14159265359f * float(i) / float(segments);
        float x = cx + r * cosf(a);
        float y = cy + r * sinf(a);
        glVertex2f(x, y);
    }
    glEnd();
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "LOD Visualiser (GLFW)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Simple loop: draw concentric rings representing LOD thresholds
    while (!glfwWindowShouldClose(window)) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        glClearColor(0.15f, 0.08f, 0.18f, 1.0f); // dark background
        glClear(GL_COLOR_BUFFER_BIT);

        // Setup simple orthographic projection -1..1
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = width / (float)height;
        if (aspect >= 1.0f)
            glOrtho(-aspect, aspect, -1.0, 1.0, -1.0, 1.0);
        else
            glOrtho(-1.0, 1.0, -1.0f/aspect, 1.0f/aspect, -1.0, 1.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Draw rings for LOD levels (closest = smallest radius)
        const int segments = 128;
        const float radii[] = {0.15f, 0.35f, 0.6f, 0.85f};
        const float colors[5][3] = {
            {0.9f, 0.2f, 0.2f}, // LOD0 - red
            {0.9f, 0.6f, 0.2f}, // LOD1 - orange
            {0.9f, 0.9f, 0.2f}, // LOD2 - yellow
            {0.2f, 0.7f, 0.3f}, // LOD3 - green
            {0.4f, 0.6f, 0.9f}  // LOD4 - blue
        };

        for (int i = 0; i < 4; ++i) {
            glColor3f(colors[i][0], colors[i][1], colors[i][2]);
            drawCircle(0.0f, 0.0f, radii[i], segments);
        }

        // Draw crosshair center
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINES);
        glVertex2f(-0.02f, 0.0f); glVertex2f(0.02f, 0.0f);
        glVertex2f(0.0f, -0.02f); glVertex2f(0.0f, 0.02f);
        glEnd();

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Close on ESC
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
