// CG_Project.cpp : This file contains the 'main' function. Program execution begins and ends there.

// Complex 3D Gravity Balls Simulator (700+ LOC)

#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ------------------ Vec3 -------------------
// 3D vector class for position and velocity
struct Vec3 {
    float x, y, z;

    // Constructor
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

    // Operator overloads for vector arithmetic
    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
    Vec3& operator+=(const Vec3& b) {
        x += b.x;
        y += b.y;
        z += b.z;
        return *this;
    }

    // Normalization and length calculation
    float length() const { return sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const {
        float l = length();
        return l > 0 ? *this / l : Vec3();
    }

    // Dot product and reflection
    float dot(const Vec3& b) const { return x * b.x + y * b.y + z * b.z; }
    Vec3 reflect(const Vec3& n) const { return *this - n * 2.0f * this->dot(n); }
};

// ------------------ Global Config Variables -------------------
float boxSize = 10.0f;
float globalGravity = -9.8f;
float globalFriction = 0.1f;
float restitution = 0.9f;
float entropyLevel = 0.0f;
float timeScale = 1.0f;
bool paused = false;
bool showUI = true;
bool wallsAreMagnetic = false;
bool blackHoleMode = false;
bool cursorGravityMode = false;
int maxParticles = 100;
int mouseX = 0, mouseY = 0;

// ------------------ Particles -------------------
Vec3 cursorWorldTarget(0, 0, 0);

// ------------------ Input and Camera -------------------
float camAngleX = 45, camAngleY = 30;
float camDist = 40.0f;
int lastMouseX = -1, lastMouseY = -1;
bool mouseLeftDown = false;

// ------------------ Time -------------------
float lastTime = 0;

// ------------------ Sparkles -------------------
struct Spark {
    Vec3 pos, vel;
    float life;
    float r, g, b;

    Spark(Vec3 p, Vec3 v) : pos(p), vel(v), life(1.0f) {
        r = 1.0f;
        g = 0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
        b = 0.0f;
    }

    void update(float dt) {
        life -= dt;
        vel += Vec3(0, globalGravity, 0) * dt * 0.1f;
        pos += vel * dt;
    }

    void draw() const {
        glColor4f(r, g, b, life);
        glPointSize(3.0f);
        glBegin(GL_POINTS);
        glVertex3f(pos.x, pos.y, pos.z);
        glEnd();
    }
};

// ------------------ Trail -------------------
struct Trail {
    std::vector<Vec3> points;
    void add(Vec3 pos) {
        points.push_back(pos);
        if (points.size() > 30)
            points.erase(points.begin());
    }

    void draw() const {
        glBegin(GL_LINE_STRIP);
        for (size_t i = 0; i < points.size(); ++i) {
            float alpha = float(i) / points.size();
            float width = 5.0f + 5.0f * alpha;
            glColor4f(1.0f, 1.0f - alpha, 1.0f, alpha);
            glVertex3f(points[i].x, points[i].y, points[i].z);
        }
        glEnd();
    }
};

// ------------------ Ball -------------------
struct Ball {
    Vec3 pos, vel;
    float radius, mass;
    float r, g, b;
    Trail trail;

    Ball(Vec3 p, Vec3 v, float radius = 0.5f) : pos(p), vel(v), radius(radius) {
        mass = radius * radius * radius;
        r = static_cast<float>(rand()) / RAND_MAX;
        g = static_cast<float>(rand()) / RAND_MAX;
        b = static_cast<float>(rand()) / RAND_MAX;
    }

    void update(float dt) {
        if (blackHoleMode) {
            Vec3 toCenter = Vec3(0, 0, 0) - pos;
            float distSq = toCenter.length();
            distSq = std::max(1.0f, distSq);
            Vec3 pull = toCenter.normalized() * (100.0f / distSq);
            vel += pull * dt;
        }

        if (cursorGravityMode) {
            Vec3 toCursor = cursorWorldTarget - pos;
            float distSq = toCursor.length();
            if (distSq > 0.5f) {
                Vec3 pull = toCursor.normalized() * (200.0f / distSq);
                vel += pull * dt;
            }
        }

        if (wallsAreMagnetic) {
            float pullStrength = 200.0f;
            float wallDist = boxSize;

            Vec3 force(0, 0, 0);
            float dX1 = fabs(-wallDist - pos.x);
            float dX2 = fabs(wallDist - pos.x);
            float dY1 = fabs(-wallDist - pos.y);
            float dY2 = fabs(wallDist - pos.y);
            float dZ1 = fabs(-wallDist - pos.z);
            float dZ2 = fabs(wallDist - pos.z);

            force.x += 1.0f / (dX1 * dX1 + 0.1f);
            force.x -= 1.0f / (dX2 * dX2 + 0.1f);

            force.y += 1.0f / (dY1 * dY1 + 0.1f);
            force.y -= 1.0f / (dY2 * dY2 + 0.1f);

            force.z += 1.0f / (dZ1 * dZ1 + 0.1f);
            force.z -= 1.0f / (dZ2 * dZ2 + 0.1f);

            vel += force * pullStrength * dt;
        }

        vel += Vec3(0, globalGravity, 0) * dt;
        vel = vel * (1.0f - globalFriction * dt);
        pos += vel * dt;
        trail.add(pos);
        vel.x += ((rand() % 200 - 100) / 100.0f) * entropyLevel * 100.0f * dt;
        vel.y += ((rand() % 200 - 100) / 100.0f) * entropyLevel * 100.0f * dt;
        vel.z += ((rand() % 200 - 100) / 100.0f) * entropyLevel * 100.0f * dt;
    }

    void draw() const {
        glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        glColor3f(r, g, b);
        glutSolidSphere(radius, 16, 16);
        glPopMatrix();
        trail.draw();
    }
};

std::vector<Ball> balls;
std::vector<Spark> sparks;

// ------------------ Simulation -------------------
void spawnSparkExplosion(Vec3 position, int count = 10) {
    for (int i = 0; i < count; ++i) {
        Vec3 dir(((rand() % 200) - 100) / 100.0f, ((rand() % 200) - 100) / 100.0f, ((rand() % 200) - 100) / 100.0f);
        sparks.emplace_back(position, dir * 3.0f);
    }
}

// ------------------ Collision Handling -------------------
// This function handles the collision detection and response between balls and walls
void handleCollisions() {
    for (size_t i = 0; i < balls.size(); ++i) {
        Ball& A = balls[i];

        // Wall collisions
        for (int j = 0; j < 3; ++j) {
            float* coord = j == 0 ? &A.pos.x : j == 1 ? &A.pos.y : &A.pos.z;
            float* vel = j == 0 ? &A.vel.x : j == 1 ? &A.vel.y : &A.vel.z;
            if (*coord - A.radius < -boxSize) {
                *coord = -boxSize + A.radius;
                *vel *= -restitution;
            }
            if (*coord + A.radius > boxSize) {
                *coord = boxSize - A.radius;
                *vel *= -restitution;
            }
        }

        // Ball-to-ball collisions
        for (size_t j = i + 1; j < balls.size(); ++j) {
            Ball& B = balls[j];
            Vec3 delta = B.pos - A.pos;
            float dist = delta.length();
            float minDist = A.radius + B.radius;
            if (dist < minDist && dist > 0) {
                Vec3 normal = delta.normalized();
                float overlap = 0.5f * (minDist - dist);

                A.pos = A.pos - (normal * overlap);
                B.pos += normal * overlap;

                Vec3 relVel = B.vel - A.vel;
                float velAlongNormal = relVel.dot(normal);
                if (velAlongNormal < 0) {
                    float impulse = -(1 + restitution) * velAlongNormal;
                    impulse /= (1 / A.mass + 1 / B.mass);
                    Vec3 impulseVec = normal * impulse;

                    A.vel = A.vel - (impulseVec / A.mass);
                    B.vel += impulseVec / B.mass;

                    spawnSparkExplosion((A.pos + B.pos) * 0.5f, 15);
                }
            }
        }
    }
}

// ------------------ Simulation Update ------------------- 
// This function is called every frame to update the simulation state
void updateSimulation(float dt) {
    if (paused)
        return;

    if (blackHoleMode) {
        for (size_t i = 0; i < balls.size();) {
            if (balls[i].pos.length() < 1.0f) {
                if (balls[i].pos.length() < 1.0f) {
                    spawnSparkExplosion(balls[i].pos, 20);
                    balls.erase(balls.begin() + i);
                    continue;
                }
                continue;
            }
            ++i;
        }
    }

    for (auto& b : balls)
        b.update(dt);
    handleCollisions();

    for (size_t i = 0; i < sparks.size();) {
        sparks[i].update(dt);
        if (sparks[i].life <= 0)
            sparks.erase(sparks.begin() + i);
        else
            ++i;
    }
}

// ------------------ Set Background -------------------
void drawBackgroundGradient() {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 1, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_QUADS);

    if (blackHoleMode) {
        glColor3f(0.02f, 0.02f, 0.04f);
        glVertex2f(0, 1);
        glColor3f(0.03f, 0.03f, 0.07f);
        glVertex2f(1, 1);
        glColor3f(0.05f, 0.04f, 0.08f);
        glVertex2f(1, 0);
        glColor3f(0.04f, 0.03f, 0.07f);
        glVertex2f(0, 0);
    }
    else if (wallsAreMagnetic) {
        glColor3f(0.08f, 0.02f, 0.12f);
        glVertex2f(0, 1);
        glColor3f(0.15f, 0.03f, 0.20f);
        glVertex2f(1, 1);
        glColor3f(0.12f, 0.00f, 0.10f);
        glVertex2f(1, 0);
        glColor3f(0.06f, 0.01f, 0.08f);
        glVertex2f(0, 0);
    }
    else if (cursorGravityMode) {
        glColor3f(0.02f, 0.07f, 0.10f);
        glVertex2f(0, 1);
        glColor3f(0.03f, 0.09f, 0.13f);
        glVertex2f(1, 1);
        glColor3f(0.04f, 0.12f, 0.16f);
        glVertex2f(1, 0);
        glColor3f(0.03f, 0.10f, 0.14f);
        glVertex2f(0, 0);
    }
    else {
        glColor3f(0.07f, 0.02f, 0.15f);
        glVertex2f(0, 1);
        glColor3f(0.07f, 0.02f, 0.15f);
        glVertex2f(1, 1);
        glColor3f(0.02f, 0.1f, 0.12f);
        glVertex2f(1, 0);
        glColor3f(0.02f, 0.1f, 0.12f);
        glVertex2f(0, 0);
    }

    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

// ------------------ Draw box -------------------
void drawBox(float size) {
    float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    float pulse = 0.9f + 0.5f * sin(t * 2.0f);

    float r = 0.0f, g = 1.0f * pulse, b = 1.0f * pulse;

    if (blackHoleMode) {
        r = 0.2f * pulse;
        g = 0.0f;
        b = 0.5f + 0.5f * pulse;
    }
    else if (wallsAreMagnetic) {
        r = 1.0f * pulse;
        g = 0.2f;
        b = 1.0f * pulse;
    }
    else if (cursorGravityMode) {
        r = 0.0f;
        g = 1.0f;
        b = 0.4f + 0.4f * sin(t * 3);
    }

    glColor3f(r, g, b);
    glLineWidth(2.5f);
    glutWireCube(size * 2.0f);
    glLineWidth(1.0f);
}

// ------------------ UI-Text Rendering -------------------
void renderText(float x, float y, const std::string& text) {
    glRasterPos2f(x, y);
    for (char c : text)
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, c);
}

// ------------------ UI Rendering -------------------
// Render the UI with stats and controls
void renderUI() {
    if (!showUI)
        return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glColor3f(1, 1, 1);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    // Line 1 – Core Stats
    oss << "Gravity [2/8]: " << globalGravity << "    ";
    oss << "Friction [4/6]: " << globalFriction << "    ";
    oss << "Elasticity [A/D]: " << restitution << "    ";
    oss << "Entropy [Q/E]: " << entropyLevel << "    ";
    oss << "Balls: " << balls.size() << "    ";
    oss << "Time Scale [</>]: " << std::fixed << std::setprecision(1) << timeScale;
    std::string line1 = oss.str();

    // Line 2 – Modes + Controls
    std::ostringstream oss2;
    oss2 << "[M] Magnetize Walls: " << (wallsAreMagnetic ? "ON" : "OFF") << "    ";
    oss2 << "[B] Black Hole: " << (blackHoleMode ? "ON" : "OFF") << "    ";
    oss2 << "[G] Cursor Gravity: " << (cursorGravityMode ? "ON" : "OFF") << "    ";
    oss2 << "Zoom [+/-]: " << static_cast<int>(camDist) << "    ";
    oss2 << "[SPACE] Pause  [R] Reset  [C] Clear  [N] New Ball  [T] UI  [ESC] Quit";
    std::string line2 = oss2.str();

    // Render lines
    float y = 580;
    renderText(10, y, "Gravity Balls 3D - Complex Mode"); y -= 15;
    renderText(10, y, line1); y -= 15;
    renderText(10, y, line2);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ------------------ Render Scene -------------------
// Main rendering function
void renderScene() {
    float t = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float rawDt = t - lastTime;
    lastTime = t;
    float dt = rawDt * timeScale;

    updateSimulation(dt);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawBackgroundGradient();

    glLoadIdentity();

    glTranslatef(0, 0, -camDist);
    glRotatef(camAngleY, 1, 0, 0);
    glRotatef(camAngleX, 0, 1, 0);

    glEnable(GL_LIGHTING);
    GLfloat light_pos[] = { 0.0f, 20.0f, 20.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glEnable(GL_LIGHT0);

    drawBox(boxSize);

    if (blackHoleMode) {
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.0f);
        float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
        float pulse = 0.6f + 0.4f * sin(t * 4.0f);

        glColor3f(0.0f, 0.0f, 0.0f);
        glutSolidSphere(1.0f, 32, 32);
        glPushMatrix();
        glRotatef(t * 100.0f, 0.0f, 1.0f, 0.0f);
        glColor4f(1.0f, 0.6f, 0.2f, 0.15f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 100; ++i) {
            float angle = 2.0f * M_PI * i / 100;
            float x = cos(angle) * 1.5f;
            float z = sin(angle) * 1.5f;
            glVertex3f(x, 0.1f, z);
        }
        glEnd();
        glPopMatrix();

        glColor4f(0.6f, 0.1f, 1.0f, 0.08f * pulse);
        glutSolidSphere(1.6f + 0.1f * sin(t * 3.0f), 32, 32);

        glPopMatrix();
    }

    if (cursorGravityMode) {
        GLdouble model[16], proj[16];
        GLint viewport[4];
        glGetDoublev(GL_MODELVIEW_MATRIX, model);
        glGetDoublev(GL_PROJECTION_MATRIX, proj);
        glGetIntegerv(GL_VIEWPORT, viewport);

        float winX = mouseX;
        float winY = mouseY;
        float winZ = 0.5f;

        GLdouble posX, posY, posZ;
        gluUnProject(winX, winY, winZ, model, proj, viewport, &posX, &posY, &posZ);
        cursorWorldTarget = Vec3(posX, posY, posZ);
    }

    for (const auto& b : balls)
        b.draw();

    glDisable(GL_LIGHTING);
    for (const auto& s : sparks)
        s.draw();

    renderUI();

    glutSwapBuffers();
}

// ------------------ Input Handling -------------------
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON)
        mouseLeftDown = (state == GLUT_DOWN);
    lastMouseX = x;
    lastMouseY = y;
}

//  ------------------ Passive Motion -------------------
void passiveMotion(int x, int y) {
    mouseX = x;
    mouseY = y;
}

//  ------------------ Motion -------------------
void motion(int x, int y) {
    mouseX = x;
    mouseY = y;

    if (cursorGravityMode) {
        GLdouble model[16], proj[16];
        GLint viewport[4];
        glGetDoublev(GL_MODELVIEW_MATRIX, model);
        glGetDoublev(GL_PROJECTION_MATRIX, proj);
        glGetIntegerv(GL_VIEWPORT, viewport);

        float winX = x;
        float winY = y;
        float winZ;
        glReadPixels(x, int(winY), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ);

        GLdouble posX, posY, posZ;
        gluUnProject(winX, winY, 0.5f, model, proj, viewport, &posX, &posY, &posZ);

        cursorWorldTarget = Vec3(posX, posY, posZ);
    }

    if (mouseLeftDown) {
        camAngleX += (x - lastMouseX) * 0.5f;
        camAngleY += (y - lastMouseY) * 0.5f;
        if (camAngleY < -89.0f)
            camAngleY = -89.0f;
        if (camAngleY > 89.0f)
            camAngleY = 89.0f;
        lastMouseX = x;
        lastMouseY = y;
    }
}

// ------------------ Keyboard Input -------------------
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        // --- Simulation Control ---
    case ' ':
        paused = !paused;
        break;
    case 'r': {
        balls.clear();
        for (int i = 0; i < 20; ++i) {
            Vec3 pos(rand() % 10 - 5, rand() % 10 + 5, rand() % 10 - 5);
            Vec3 vel((rand() % 100 - 50) / 50.0f, 0, (rand() % 100 - 50) / 50.0f);
            balls.emplace_back(pos, vel, 0.4f + rand() % 10 / 20.0f);
        }
        break;
    }
    case 'c':
        balls.clear();
        sparks.clear();
        break;
    case '<': case ',':
        timeScale = std::max(0.1f, timeScale - 0.1f);
        break;
    case '>': case '.':
        timeScale = std::min(5.0f, timeScale + 0.1f);
        break;

        // --- Camera Control ---
    case '+': case '=':
        camDist = std::max(5.0f, camDist - 1.0f);
        break;
    case '-': case '_':
        camDist = std::min(100.0f, camDist + 1.0f);
        break;

        // --- New Ball ---
    case 'n': {
        Vec3 pos(rand() % 10 - 5, 10 + rand() % 5, rand() % 10 - 5);
        Vec3 vel((rand() % 100 - 50) / 50.0f, 0, (rand() % 100 - 50) / 50.0f);
        balls.emplace_back(pos, vel, 0.5f + (rand() % 10) / 20.0f);
        break;
    }

            // --- Physics: Gravity, Friction, Entropy & Elasticity ---
    case '2':
        globalGravity -= 1.0f;
        break;
    case '8':
        globalGravity += 1.0f;
        break;
    case '4':
        globalFriction = std::max(0.0f, globalFriction - 0.01f);
        break;
    case '6':
        globalFriction += 0.01f;
        break;
    case 'q':
        entropyLevel = std::max(0.0f, entropyLevel - 0.01f);
        break;
    case 'e':
        entropyLevel += 0.01f;
        break;
    case 'a':
        restitution = std::max(0.0f, restitution - 0.05f);
        break;
    case 'd':
        restitution = std::min(1.0f, restitution + 0.05f);
        break;

        // --- Modes ---
    case 'm':
        wallsAreMagnetic = !wallsAreMagnetic;
        break;
    case 'b':
        blackHoleMode = !blackHoleMode;
        break;
    case 'g':
        cursorGravityMode = !cursorGravityMode;
        break;

        // --- Toggle UI and Exit ---

    case 't':
        showUI = !showUI;
        break;
    case 27:
        exit(0);
        break;
    }
}

// ------------------ Reshape Window -------------------
// This function is called when the window is resized, adjusting the viewport and projection matrix.
void reshape(int w, int h) {
    if (h == 0)
        h = 1;
    float aspect = float(w) / h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, aspect, 1.0f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

// ------------------ Idle Function -------------------
// This function is called when the program is idle, allowing for continuous updates.
void idle() { glutPostRedisplay(); }

// ------------------ Main Entry Point -------------------
// The main function initializes GLUT, sets up the window, and enters the main loop.
int main(int argc, char** argv) {

    // Initialize GLUT and create a window
    srand((unsigned int)time(0));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1000, 700);
    glutCreateWindow("Gravity Balls 3D");

    // Set up OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // Set up initial state
    for (int i = 0; i < 20; ++i) {
        Vec3 pos(rand() % 10 - 5, rand() % 10 + 5, rand() % 10 - 5);
        Vec3 vel((rand() % 100 - 50) / 50.0f, 0, (rand() % 100 - 50) / 50.0f);
        balls.emplace_back(pos, vel, 0.4f + rand() % 10 / 20.0f);
    }

    // Set up callbacks
    glutDisplayFunc(renderScene);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(passiveMotion);

    glutMainLoop();
    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
