#ifndef CORE_H
#define CORE_H

// Core - Rubik's cube simulation + animation helper
// - Keeps logical sticker state (54 stickers)
// - Supports queued moves like "R", "U'", "F2"
// - Produces per-sticker model matrices and colors so your renderer (OpenGL+GLFW+GLAD)
//   can draw each sticker (or each cubie face) using your existing draw code.
// Usage:
//   Core core;
//   // each frame:
//   core.update(deltaSeconds);
//   auto mats = core.getStickerModelMatrices(); // 54 matrices
//   auto cols = core.getStickerColors();       // 54 colors
//   // feed mats/cols to your draw path
//
// Requires GLM (vec/mat/quaternion). No GLFW/glad calls here.

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct StickerTransform {
    glm::mat4 model;
    glm::vec3 color;
};

class Core {
public:
    // cubieSize: length of each small cube (default 1.0)
    // gap: spacing between cubelets (small gap to see seams)
    // animSpeedDegPerSec: rotation speed in degrees/sec (default 360 => 90deg in 0.25s)
    Core(float cubieSize = 1.0f, float gap = 0.03f, float animSpeedDegPerSec = 360.0f);

    // Call every frame with seconds elapsed since last frame
    void update(float deltaSeconds);

    // Queue a move: "U", "U'", "U2", "R", "R'", "F2", etc.
    // Accepts moves for: U D L R F B
    // Returns true if accepted
    bool queueMove(const std::string& move);

    // Start a move immediately (clears current animation queue and starts this)
    bool startMoveImmediate(const std::string& move);

    // Are we currently animating a rotation?
    bool isAnimating() const;

    // Get transforms & colors for all 54 stickers (in fixed order: U(9), R(9), F(9), D(9), L(9), B(9))
    // The order is stable but you can just iterate them together.
    std::vector<glm::mat4> getStickerModelMatrices();
    std::vector<glm::vec3> getStickerColors();

    // Clear queued moves
    void clearQueue();

private:
    struct Sticker {
        glm::ivec3 cubePos; // each component in {-1,0,1}
        glm::ivec3 normal;  // one of axis unit vectors (e.g. (0,1,0))
        glm::vec3 color;
        glm::mat4 baseModel; // model transform when idle (no ongoing animation)
    };

    // current rotation animation state
    struct Anim {
        bool active = false;
        glm::vec3 axis = glm::vec3(0.0f);
        int layer = 0;           // -1, 0, +1 for the layer coordinate along axis
        float targetAngle = 0.0f;// degrees (±90 or 180)
        float currentAngle = 0.0f;
        float speedDeg = 360.0f; // deg per second
    };


    float m_cubieSize;
    float m_gap;
    float m_spacing; 
    Anim m_anim;
    std::vector<std::string> m_queue;

    std::vector<Sticker> m_stickers;                       

 
    void buildInitialStickers();
    void rebuildBaseModel(Sticker& s);
    void startParsedMove(char face, int amount, bool prime);
    bool parseMove(const std::string& move, char& face, int& amount, bool& prime);
    void applyRotationDiscrete(const glm::vec3& axis, int layer, float angleDeg);
    glm::vec3 faceToColor(char face) const;
    void startNextInQueue();
};

#endif // CORE_H