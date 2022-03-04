////////////////////////////////////////////////////////////////////////
//
//   Harvard University
//   CS175 : Computer Graphics
//   Professor Steven Gortler
//
////////////////////////////////////////////////////////////////////////

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define GLEW_STATIC


#include "GL/glew.h"
#include "GL/glfw3.h"

#include "cvec.h"
#include "geometrymaker.h"
#include "glsupport.h"
#include "matrix4.h"
#include "ppm.h"

using namespace std; // for string, vector, iostream, and other standard C++ stuff

// G L O B A L S ///////////////////////////////////////////////////

// --------- IMPORTANT --------------------------------------------------------
// Before you start working on this assignment, set the following variable
// properly to indicate whether you want to use OpenGL 2.x with GLSL 1.0 or
// OpenGL 3.x+ with GLSL 1.5.
//
// Set g_Gl2Compatible = true to use GLSL 1.0 and g_Gl2Compatible = false to
// use GLSL 1.5. Use GLSL 1.5 unless your system does not support it.
//
// If g_Gl2Compatible=true, shaders with -gl2 suffix will be loaded.
// If g_Gl2Compatible=false, shaders with -gl3 suffix will be loaded.
// To complete the assignment you only need to edit the shader files that get
// loaded
// ----------------------------------------------------------------------------
static const bool g_Gl2Compatible = false;

static const float g_frustMinFov = 60.0; // A minimal of 60 degree field of view
static float g_frustFovY = g_frustMinFov; // FOV in y direction (updated by updateFrustFovY)

static const float g_frustNear = -0.1;  // near plane
static const float g_frustFar = -50.0;  // far plane
static const float g_groundY = -2.0;    // y coordinate of the ground
static const float g_groundSize = 10.0; // half the ground length

static GLFWwindow* g_window;

static int g_windowWidth = 512;
static int g_windowHeight = 512;
static double g_wScale = 1;
static double g_hScale = 1;

static bool g_mouseClickDown = false; // is the mouse button pressed
static bool g_mouseLClickButton, g_mouseRClickButton, g_mouseMClickButton;
static bool g_spaceDown = false; // space state, for middle mouse emulation
static int g_mouseClickX, g_mouseClickY; // coordinates for mouse click event
static int g_activeShader = 0;

struct ShaderState {
    GlProgram program;

    // Handles to uniform variables
    GLint h_uLight, h_uLight2;
    GLint h_uProjMatrix;
    GLint h_uModelViewMatrix;
    GLint h_uNormalMatrix;
    GLint h_uColor;

    // Handles to vertex attributes
    GLint h_aPosition;
    GLint h_aNormal;

    ShaderState(const char *vsfn, const char *fsfn) {
        readAndCompileShader(program, vsfn, fsfn);

        const GLuint h = program; // short hand

        // Retrieve handles to uniform variables
        h_uLight = safe_glGetUniformLocation(h, "uLight");
        h_uLight2 = safe_glGetUniformLocation(h, "uLight2");
        h_uProjMatrix = safe_glGetUniformLocation(h, "uProjMatrix");
        h_uModelViewMatrix = safe_glGetUniformLocation(h, "uModelViewMatrix");
        h_uNormalMatrix = safe_glGetUniformLocation(h, "uNormalMatrix");
        h_uColor = safe_glGetUniformLocation(h, "uColor");

        // Retrieve handles to vertex attributes
        h_aPosition = safe_glGetAttribLocation(h, "aPosition");
        h_aNormal = safe_glGetAttribLocation(h, "aNormal");

        if (!g_Gl2Compatible)
            glBindFragDataLocation(h, 0, "fragColor");
        checkGlErrors();
    }
};

static const int g_numShaders = 2;
static const char *const g_shaderFiles[g_numShaders][2] = {
    {"./shaders/basic-gl3.vshader", "./shaders/diffuse-gl3.fshader"},
    {"./shaders/basic-gl3.vshader", "./shaders/solid-gl3.fshader"}};
static const char *const g_shaderFilesGl2[g_numShaders][2] = {
    {"./shaders/basic-gl2.vshader", "./shaders/diffuse-gl2.fshader"},
    {"./shaders/basic-gl2.vshader", "./shaders/solid-gl2.fshader"}};
static vector<shared_ptr<ShaderState>>
    g_shaderStates; // our global shader states

// --------- Geometry

// Macro used to obtain relative offset of a field within a struct
#define FIELD_OFFSET(StructType, field) &(((StructType *)0)->field)

// A vertex with floating point position and normal
struct VertexPN {
    Cvec3f p, n;

    VertexPN() {}
    VertexPN(float x, float y, float z, float nx, float ny, float nz)
        : p(x, y, z), n(nx, ny, nz) {}

    // Define copy constructor and assignment operator from GenericVertex so we
    // can use make* functions from geometrymaker.h
    VertexPN(const GenericVertex &v) { *this = v; }

    VertexPN &operator=(const GenericVertex &v) {
        p = v.pos;
        n = v.normal;
        return *this;
    }
};

struct Geometry {
    GlBufferObject vbo, ibo;
    GlArrayObject vao;
    int vboLen, iboLen;

    Geometry(VertexPN *vtx, unsigned short *idx, int vboLen, int iboLen) {
        this->vboLen = vboLen;
        this->iboLen = iboLen;

        // Now create the VBO and IBO
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPN) * vboLen, vtx,
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * iboLen,
                     idx, GL_STATIC_DRAW);
    }

    void draw(const ShaderState &curSS) {
        // bind the object's VAO
        glBindVertexArray(vao);

        // Enable the attributes used by our shader
        safe_glEnableVertexAttribArray(curSS.h_aPosition);
        safe_glEnableVertexAttribArray(curSS.h_aNormal);

        // bind vbo
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        safe_glVertexAttribPointer(curSS.h_aPosition, 3, GL_FLOAT, GL_FALSE,
                                   sizeof(VertexPN), FIELD_OFFSET(VertexPN, p));
        safe_glVertexAttribPointer(curSS.h_aNormal, 3, GL_FLOAT, GL_FALSE,
                                   sizeof(VertexPN), FIELD_OFFSET(VertexPN, n));

        // bind ibo
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

        // draw!
        glDrawElements(GL_TRIANGLES, iboLen, GL_UNSIGNED_SHORT, 0);

        // Disable the attributes used by our shader
        safe_glDisableVertexAttribArray(curSS.h_aPosition);
        safe_glDisableVertexAttribArray(curSS.h_aNormal);

        // disable VAO
        glBindVertexArray((GLuint) NULL);
    }
};

// Vertex buffer and index buffer associated with the ground and cube geometry
static shared_ptr<Geometry> g_ground, g_cube;

// --------- Scene

static const Cvec3 g_light1(2.0, 3.0, 14.0),
    g_light2(-2, -3.0, -5.0); // define two lights positions in world space
static Matrix4 g_skyRbt = Matrix4::makeTranslation(Cvec3(0.0, 0.25, 4.0));
static Matrix4 g_objectRbt[2] = 
    {
        Matrix4::makeTranslation(Cvec3(-1, 0, 0)),
        Matrix4::makeTranslation(Cvec3(1, 0, 0))
    }; 
// currently only 1 obj is defined -> now 2 objs are defined!
static Cvec3f g_objectColors[2] = 
    {
        Cvec3f(1, 0, 0),
        Cvec3f(0, 0, 1)
    };


// ----------- More Scene

static int currentView = 0;
static int currentObj = 0;
static int currentMode = 0;

///////////////// END OF G L O B A L S
/////////////////////////////////////////////////////

static void initGround() {
    // A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2
    VertexPN vtx[4] = {
        VertexPN(-g_groundSize, g_groundY, -g_groundSize, 0, 1, 0),
        VertexPN(-g_groundSize, g_groundY, g_groundSize, 0, 1, 0),
        VertexPN(g_groundSize, g_groundY, g_groundSize, 0, 1, 0),
        VertexPN(g_groundSize, g_groundY, -g_groundSize, 0, 1, 0),
    };
    unsigned short idx[] = {0, 1, 2, 0, 2, 3};
    g_ground.reset(new Geometry(&vtx[0], &idx[0], 4, 6));
}

static void initCubes() {
    int ibLen, vbLen;
    getCubeVbIbLen(vbLen, ibLen);

    // Temporary storage for cube geometry
    vector<VertexPN> vtx(vbLen);
    vector<unsigned short> idx(ibLen);

    makeCube(1, vtx.begin(), idx.begin());
    g_cube.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));
}

// takes a projection matrix and send to the the shaders
static void sendProjectionMatrix(const ShaderState &curSS,
                                 const Matrix4 &projMatrix) {
    GLfloat glmatrix[16];
    projMatrix.writeToColumnMajorMatrix(glmatrix); // send projection matrix
    safe_glUniformMatrix4fv(curSS.h_uProjMatrix, glmatrix);
}

// takes MVM and its normal matrix to the shaders
static void sendModelViewNormalMatrix(const ShaderState &curSS,
                                      const Matrix4 &MVM, const Matrix4 &NMVM) {
    GLfloat glmatrix[16];
    MVM.writeToColumnMajorMatrix(glmatrix); // send MVM
    safe_glUniformMatrix4fv(curSS.h_uModelViewMatrix, glmatrix);

    NMVM.writeToColumnMajorMatrix(glmatrix); // send NMVM
    safe_glUniformMatrix4fv(curSS.h_uNormalMatrix, glmatrix);
}

// update g_frustFovY from g_frustMinFov, g_windowWidth, and g_windowHeight
static void updateFrustFovY() {
    if (g_windowWidth >= g_windowHeight)
        g_frustFovY = g_frustMinFov;
    else {
        const double RAD_PER_DEG = 0.5 * CS175_PI / 180;
        g_frustFovY = atan2(sin(g_frustMinFov * RAD_PER_DEG) * g_windowHeight /
                                g_windowWidth,
                            cos(g_frustMinFov * RAD_PER_DEG)) /
                      RAD_PER_DEG;
    }
}

static Matrix4 makeProjectionMatrix() {
    return Matrix4::makeProjection(
        g_frustFovY, g_windowWidth / static_cast<double>(g_windowHeight),
        g_frustNear, g_frustFar);
}

static void drawStuff() {
    // short hand for current shader state
    const ShaderState &curSS = *g_shaderStates[g_activeShader];

    // build & send proj. matrix to vshader
    const Matrix4 projmat = makeProjectionMatrix();
    sendProjectionMatrix(curSS, projmat);

    Matrix4 eyeRbt;
    // use the skyRbt as the eyeRbt
    if(currentView == 0) // sky
        eyeRbt = g_skyRbt;
    else if(currentView == 1) // object1
        eyeRbt = g_objectRbt[0];
    else // object2
        eyeRbt = g_objectRbt[1];

    const Matrix4 invEyeRbt = inv(eyeRbt);

    const Cvec3 eyeLight1 = Cvec3(
        invEyeRbt * Cvec4(g_light1, 1)); // g_light1 position in eye coordinates
    const Cvec3 eyeLight2 = Cvec3(
        invEyeRbt * Cvec4(g_light2, 1)); // g_light2 position in eye coordinates
    safe_glUniform3f(curSS.h_uLight, eyeLight1[0], eyeLight1[1], eyeLight1[2]);
    safe_glUniform3f(curSS.h_uLight2, eyeLight2[0], eyeLight2[1], eyeLight2[2]);

    // draw ground
    // ===========
    //
    const Matrix4 groundRbt = Matrix4(); // identity
    Matrix4 MVM = invEyeRbt * groundRbt;
    Matrix4 NMVM = normalMatrix(MVM);
    sendModelViewNormalMatrix(curSS, MVM, NMVM);
    safe_glUniform3f(curSS.h_uColor, 0.1, 0.95, 0.1); // set color
    g_ground->draw(curSS);

    // draw cubes
    // ==========
    MVM = invEyeRbt * g_objectRbt[0];
    NMVM = normalMatrix(MVM);
    sendModelViewNormalMatrix(curSS, MVM, NMVM);
    safe_glUniform3f(curSS.h_uColor, g_objectColors[0][0], g_objectColors[0][1],
                     g_objectColors[0][2]);
    g_cube->draw(curSS);

    MVM = invEyeRbt * g_objectRbt[1];
    NMVM = normalMatrix(MVM);
    sendModelViewNormalMatrix(curSS, MVM, NMVM);
    safe_glUniform3f(curSS.h_uColor, g_objectColors[1][0], g_objectColors[1][1],
                     g_objectColors[1][2]);
    g_cube->draw(curSS);
}

static void display() {
    glUseProgram(g_shaderStates[g_activeShader]->program);
    // clear framebuffer color&depth
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawStuff();

    glfwSwapBuffers(g_window); // show the back buffer (where we rendered stuff)

    checkGlErrors();
}

static void reshape(GLFWwindow * window, const int w, const int h) {
    glfwGetFramebufferSize(g_window, &g_windowWidth, &g_windowHeight);

    glViewport(0, 0, g_windowWidth, g_windowHeight);
    cerr << "Size of window is now " << g_windowWidth << "x" << g_windowHeight << endl;
    updateFrustFovY();
}

static void motion(GLFWwindow *window, double x, double y) {
    const double dx = x - g_mouseClickX;
    const double dy = g_windowHeight - y - 1 - g_mouseClickY;

    Matrix4 m;
    Matrix4 mx, my;
    bool isTranslation;

    if (g_mouseLClickButton && !g_mouseRClickButton &&
        !g_spaceDown) { // left button down?
        m = Matrix4::makeXRotation(-dy) * Matrix4::makeYRotation(dx);
        mx = Matrix4::makeXRotation(dy);
        my = Matrix4::makeYRotation(-dx);
        isTranslation = 0;
    } else if (g_mouseRClickButton &&
               !g_mouseLClickButton) { // right button down?
        if(currentObj == 0)
            m = Matrix4::makeTranslation(Cvec3(-dx, -dy, 0) * 0.01); // is sky
        else 
            m = Matrix4::makeTranslation(Cvec3(dx, dy, 0) * 0.01); // is cube
        isTranslation = 1;
    } else if (g_mouseMClickButton ||
               (g_mouseLClickButton && g_mouseRClickButton) ||
               (g_mouseLClickButton && !g_mouseRClickButton &&
                g_spaceDown)) { // middle or (left and right, or left + space)
                                // button down?
        if(currentObj == 0)
            m = Matrix4::makeTranslation(Cvec3(0, 0, dy) * 0.01); // is sky
        else
            m = Matrix4::makeTranslation(Cvec3(0, 0, -dy) * 0.01); // is cube
        isTranslation = 1;
    }

    Matrix4 a;
    Matrix4 i; // identity matrix

    if (g_mouseClickDown) {
        // g_objectRbt[0] *= m; // Simply right-multiply is WRONG

        if(currentView == 0) // current view is NOT a cube aka. current view is sky -> then manipulation is allowed
        {
            if(currentObj == 1) // red cube
            {
                a = transFact(g_objectRbt[0]) * linFact(g_skyRbt);
                g_objectRbt[0] = a * m * inv(a) * g_objectRbt[0];
            } else if (currentObj == 2) // blue cube
            {
                a = transFact(g_objectRbt[1]) * linFact(g_skyRbt);
                g_objectRbt[1] = a * m * inv(a) * g_objectRbt[1];
            } else // sky cube
            {
                if(currentMode == 1) // ego-motion
                {
                    if(isTranslation == 1)
                    {
                        a = g_skyRbt;
                        g_skyRbt = a * m * inv(a) * g_skyRbt;
                    }
                    else
                    {
                    a = transFact(g_skyRbt);
                    g_skyRbt = a * my * inv(a) * g_skyRbt;

                    a = g_skyRbt;
                    g_skyRbt = a * mx * inv(a) * g_skyRbt;
                    }
                }
                else // 0 = orbit
                {
                    if(isTranslation == 1)
                    {
                        a = g_skyRbt;
                        g_skyRbt = a * m * inv(a) * g_skyRbt;
                    }
                    else
                    {
                        a = i;
                        g_skyRbt = a * my * inv(a) * g_skyRbt;

                        a = linFact(g_skyRbt);
                        g_skyRbt = a * mx * inv(a) * g_skyRbt;
                    }
                }
            }
        }
    }

    g_mouseClickX = x;
    g_mouseClickY = g_windowHeight - y - 1;
}

static void mouse(GLFWwindow *window, int button, int state, int mods) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    g_mouseClickX = x;
    // conversion from window-coordinate-system to OpenGL window-coordinate-system
    g_mouseClickY = g_windowHeight - y - 1;

    g_mouseLClickButton |= (button == GLFW_MOUSE_BUTTON_LEFT && state == GLFW_PRESS);
    g_mouseRClickButton |= (button == GLFW_MOUSE_BUTTON_RIGHT && state == GLFW_PRESS);
    g_mouseMClickButton |= (button == GLFW_MOUSE_BUTTON_MIDDLE && state == GLFW_PRESS);

    g_mouseLClickButton &= !(button == GLFW_MOUSE_BUTTON_LEFT && state == GLFW_RELEASE);
    g_mouseRClickButton &= !(button == GLFW_MOUSE_BUTTON_RIGHT && state == GLFW_RELEASE);
    g_mouseMClickButton &= !(button == GLFW_MOUSE_BUTTON_MIDDLE && state == GLFW_RELEASE);

    g_mouseClickDown = g_mouseLClickButton || g_mouseRClickButton || g_mouseMClickButton;
}

static void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            exit(0);
        case GLFW_KEY_H:
            cout << " ============== H E L P ==============\n\n"
                 << "h\t\thelp menu\n"
                 << "s\t\tsave screenshot\n"
                 << "f\t\tToggle flat shading on/off.\n"
                 << "o\t\tCycle object to edit\n"
                 << "v\t\tCycle view\n"
                 << "drag left mouse to rotate\n"
                 << endl;
            break;
        case GLFW_KEY_S:
            glFlush();
            writePpmScreenshot(g_windowWidth, g_windowHeight, "out.ppm");
            break;
        case GLFW_KEY_F:
            g_activeShader = (g_activeShader + 1) % g_numShaders;
            break;
        case GLFW_KEY_SPACE:
            g_spaceDown = true;
            break;
        case GLFW_KEY_O:
            currentObj++;
            currentObj = currentObj%3;
            if(currentObj == 0){
                cout << "Active object is Sky\n";
            } else if (currentObj == 1){
                cout << "Active object is Object 0\n";
            } else {
                cout << "Active object is Object 1\n";
            }
            break;
        case GLFW_KEY_V:
            currentView++;
            currentView = currentView%3;
            if(currentView == 0){
                cout << "Active eye is Sky\n";
            } else if (currentView == 1){
                cout << "Active eye is Object 0\n";
            } else {
                cout << "Active eye is Object 1\n";
            }
            break;

        case GLFW_KEY_M:
        currentMode++;
        currentMode = currentMode%2;
        if(currentMode == 1)
            cout << "Editing sky eye w.r.t. sky-sky frame\n";
        else 
            cout << "Editing sky eye w.r.t. world-sky frame\n";
        break;
        }
    } else {
        switch (key) {
        case GLFW_KEY_SPACE:
            g_spaceDown = false;
            break;
        }
    }
}

void error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

static void initGlfwState() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);

    g_window = glfwCreateWindow(g_windowWidth, g_windowHeight,
                                "Assignment 3", NULL, NULL);
    if (!g_window) {
        fprintf(stderr, "Failed to create GLFW window or OpenGL context\n");
        exit(1);
    }
    glfwMakeContextCurrent(g_window);

    glfwSwapInterval(1);

    glfwSetErrorCallback(error_callback);
    glfwSetMouseButtonCallback(g_window, mouse);
    glfwSetCursorPosCallback(g_window, motion);
    glfwSetWindowSizeCallback(g_window, reshape);
    glfwSetKeyCallback(g_window, keyboard);

}

static void initGLState() {
    glClearColor(128. / 255., 200. / 255., 255. / 255., 0.);
    glClearDepth(0.);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glReadBuffer(GL_BACK);
    if (!g_Gl2Compatible)
        glEnable(GL_FRAMEBUFFER_SRGB);
}

static void initShaders() {
    g_shaderStates.resize(g_numShaders);
    for (int i = 0; i < g_numShaders; ++i) {
        if (g_Gl2Compatible)
            g_shaderStates[i].reset(new ShaderState(g_shaderFilesGl2[i][0],
                                                    g_shaderFilesGl2[i][1]));
        else
            g_shaderStates[i].reset(
                new ShaderState(g_shaderFiles[i][0], g_shaderFiles[i][1]));
    }
}

static void initGeometry() {
    initGround();
    initCubes();
}

void glfwLoop() {
    while (!glfwWindowShouldClose(g_window)) {
        display();
        glfwWaitEvents();
    }
    printf("end loop\n");
}

int main(int argc, char *argv[]) {
    try {
        initGlfwState();

        glewInit(); // load the OpenGL extensions
#ifndef __MAC__

        if ((!g_Gl2Compatible) && !GLEW_VERSION_3_0)
            throw runtime_error("Error: card/driver does not support OpenGL "
                                "Shading Language v1.3");
        else if (g_Gl2Compatible && !GLEW_VERSION_2_0)
            throw runtime_error("Error: card/driver does not support OpenGL "
                                "Shading Language v1.0");
#endif

        cout << (g_Gl2Compatible ? "Will use OpenGL 2.x / GLSL 1.0"
                                 : "Will use OpenGL 3.x / GLSL 1.5")
             << endl;

        initGLState();
        initShaders();
        initGeometry();

        glfwLoop();
        return 0;
    } catch (const runtime_error &e) {
        cout << "Exception caught: " << e.what() << endl;
        return -1;
    }
}
