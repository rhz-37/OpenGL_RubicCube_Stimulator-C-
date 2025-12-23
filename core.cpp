#include "core.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <algorithm>

// Constants for face order in getSticker*:
// We'll create stickers in this stable order: U (y=+1), R (x=+1), F (z=+1),
// D (y=-1), L (x=-1), B (z=-1) -- each with 9 stickers row-major (-1..1 x, -1..1 z)
static const char FACE_ORDER[6] = { 'U', 'R', 'F', 'D', 'L', 'B' };

Core::Core(float cubieSize, float gap, float animSpeedDegPerSec)
    : m_cubieSize(cubieSize), m_gap(gap), m_anim(), m_spacing(cubieSize + gap)
{
    m_anim.speedDeg = animSpeedDegPerSec;
    buildInitialStickers();
}

void Core::buildInitialStickers()
{
    m_stickers.clear();
    m_stickers.reserve(54);

    // For each face in stable order create 9 stickers.
    for (int fi = 0; fi < 6; ++fi) {
        char face = FACE_ORDER[fi];
        glm::ivec3 normal(0);
        int fixCoord = 0;
        if (face == 'U') { normal = glm::ivec3(0, 1, 0); fixCoord = +1; }
        if (face == 'D') { normal = glm::ivec3(0, -1, 0); fixCoord = -1; }
        if (face == 'F') { normal = glm::ivec3(0, 0, 1); fixCoord = +1; }
        if (face == 'B') { normal = glm::ivec3(0, 0, -1); fixCoord = -1; }
        if (face == 'R') { normal = glm::ivec3(1, 0, 0); fixCoord = +1; }
        if (face == 'L') { normal = glm::ivec3(-1, 0, 0); fixCoord = -1; }

        // iterate 3x3 grid for that face
        for (int a = -1; a <= 1; ++a) {
            for (int b = -1; b <= 1; ++b) {
                Sticker s;
                s.color = faceToColor(face);
                // Determine cubePos depending on face:
                // We'll map (a,b) to the two free axes. We'll pick consistent mapping:
                // For U/D: a => x, b => -z (so top-left is (-1,1))
                // For F/B: a => x, b => -y
                // For R/L: a => z, b => -y
                if (face == 'U' || face == 'D') {
                    s.cubePos = glm::ivec3(a, fixCoord, -b);
                } else if (face == 'F' || face == 'B') {
                    s.cubePos = glm::ivec3(a, -b, fixCoord);
                } else { // R or L
                    s.cubePos = glm::ivec3(fixCoord, -b, a);
                }
                s.normal = normal;
                rebuildBaseModel(s);
                m_stickers.push_back(s);
            }
        }
    }
}

glm::vec3 Core::faceToColor(char face) const
{
    // Conventional coloring:
    // U = white, D = yellow, F = red, B = orange, R = green, L = blue
    switch (face) {
        case 'U': return glm::vec3(1.0f, 1.0f, 1.0f);
        case 'D': return glm::vec3(1.0f, 1.0f, 0.0f);
        case 'F': return glm::vec3(0.8f, 0.05f, 0.05f);
        case 'B': return glm::vec3(1.0f, 0.5f, 0.0f);
        case 'R': return glm::vec3(0.05f, 0.7f, 0.05f);
        case 'L': return glm::vec3(0.05f, 0.15f, 0.9f);
    }
    return glm::vec3(0.2f);
}

void Core::rebuildBaseModel(Sticker& s)
{
    // Build a model matrix for the sticker quad when no animation is happening.
    // Place the sticker slightly offset from the cubie surface along its normal.
    glm::vec3 pos = glm::vec3(s.cubePos) * m_spacing;
    float offset = 0.5f * m_cubieSize + 0.001f; // slightly out from cubie surface
    pos += glm::vec3(s.normal) * offset;

    // We want the sticker quad to face outward along s.normal.
    glm::vec3 zDir = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 n = glm::normalize(glm::vec3(s.normal));
    glm::quat q;
    if (glm::length(glm::cross(zDir, n)) < 1e-4f) {
        // parallel or anti-parallel
        if (glm::dot(zDir, n) > 0.0f) q = glm::quat(1,0,0,0);
        else q = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f,1.0f,0.0f));
    } else {
        float angle = acos(glm::clamp(glm::dot(zDir, n), -1.0f, 1.0f));
        glm::vec3 axis = glm::normalize(glm::cross(zDir, n));
        q = glm::angleAxis(angle, axis);
    }

    // scale of sticker quad relative to cubie face (leave tiny margin)
    float stickerScale = m_cubieSize * 0.92f;
    glm::mat4 M = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(q) * glm::scale(glm::mat4(1.0f), glm::vec3(stickerScale, stickerScale, 1.0f));
    s.baseModel = M;
}

void Core::update(float deltaSeconds)
{
    // progress animation if any
    if (m_anim.active) {
        float step = m_anim.speedDeg * deltaSeconds;
        float remaining = std::abs(m_anim.targetAngle - m_anim.currentAngle);
        float take = std::min(step, remaining);
        // advance in the sign of targetAngle
        m_anim.currentAngle += (m_anim.targetAngle >= 0.0f ? 1.0f : -1.0f) * take;

        if (std::abs(std::abs(m_anim.currentAngle) - std::abs(m_anim.targetAngle)) < 1e-3f ||
            remaining <= 1e-4f) {
            // finish
            float finalAngle = m_anim.targetAngle; // may be ±90 or 180
            applyRotationDiscrete(m_anim.axis, m_anim.layer, finalAngle);
            m_anim.active = false;
            m_anim.currentAngle = 0.0f;
            startNextInQueue();
        }
    } else {
        // if idle and queue non-empty start next
        if (!m_queue.empty()) {
            startNextInQueue();
        }
    }
}

bool Core::queueMove(const std::string& move)
{
    char face; int amount; bool prime;
    if (!parseMove(move, face, amount, prime)) return false;
    m_queue.push_back(move);
    return true;
}

bool Core::startMoveImmediate(const std::string& move)
{
    m_queue.clear();
    char face; int amount; bool prime;
    if (!parseMove(move, face, amount, prime)) return false;
    startParsedMove(face, amount, prime);
    return true;
}

void Core::startNextInQueue()
{
    if (m_anim.active) return;
    if (m_queue.empty()) return;
    std::string mv = m_queue.front();
    m_queue.erase(m_queue.begin());
    char face; int amount; bool prime;
    if (!parseMove(mv, face, amount, prime)) return;
    startParsedMove(face, amount, prime);
}

bool Core::isAnimating() const {
    return m_anim.active;
}

void Core::clearQueue() {
    m_queue.clear();
}

bool Core::parseMove(const std::string& move, char& face, int& amount, bool& prime)
{
    // expected forms: "U", "U'", "U2", optionally with whitespace trimmed
    if (move.empty()) return false;
    std::string s;
    for (char c : move) if (!isspace((unsigned char)c)) s.push_back(c);
    face = toupper(s[0]);
    if (strchr("UDLRFB", face) == nullptr) return false;
    amount = 1;
    prime = false;
    if (s.size() >= 2) {
        if (s[1] == '2') amount = 2;
        else if (s[1] == '\'' ) prime = true;
    }
    if (s.size() >= 3) {
        // e.g. "R2'" - support it (amount then prime)
        if (s[1] == '2' && s[2] == '\'') prime = true;
    }
    return true;
}

void Core::startParsedMove(char face, int amount, bool prime)
{
    // map face to axis & layer
    glm::vec3 axis(0.0f);
    int layer = 0;
    if (face == 'U') { axis = glm::vec3(0,1,0); layer = +1; }
    if (face == 'D') { axis = glm::vec3(0,1,0); layer = -1; }
    if (face == 'F') { axis = glm::vec3(0,0,1); layer = +1; }
    if (face == 'B') { axis = glm::vec3(0,0,1); layer = -1; }
    if (face == 'R') { axis = glm::vec3(1,0,0); layer = +1; }
    if (face == 'L') { axis = glm::vec3(1,0,0); layer = -1; }

    // determine total angle
    float angle = 90.0f * amount;
    if (prime) angle = -angle;
    // special-case: double-turn direction irrelevant sign, keep positive 180
    if (amount == 2) angle = 180.0f * (prime ? 1.0f : 1.0f);

    // start animation
    m_anim.active = true;
    m_anim.axis = axis;
    m_anim.layer = layer;
    m_anim.targetAngle = angle;
    m_anim.currentAngle = 0.0f;
    // speed already set in m_anim.speedDeg
}

void Core::applyRotationDiscrete(const glm::vec3& axis, int layer, float angleDeg)
{
    // Apply discrete rotation to stickers in the layer (update their logical cubePos and normal),
    // then rebuild baseModel for all stickers to reflect new resting positions.
    // We'll rotate integer positions using quaternion and rounding.

    glm::vec3 a = glm::normalize(axis);
    float rad = glm::radians(angleDeg);
    glm::quat q = glm::angleAxis(rad, a);

    for (auto &s : m_stickers) {
        // Determine sticker's integer coordinate along axis to see if it's part of the rotating layer.
        // axis is axis vector of one component 1; layer in -1..1.
        int coord = 0;
        if (std::abs(axis.x) > 0.5f) coord = s.cubePos.x;
        else if (std::abs(axis.y) > 0.5f) coord = s.cubePos.y;
        else coord = s.cubePos.z;

        if (coord != layer) continue;

        // rotate cubePos (integer vector) by quaternion and round to nearest integer
        glm::vec3 p = glm::vec3(s.cubePos);
        glm::vec3 p2 = glm::round(glm::vec3(q * p));
        s.cubePos = glm::ivec3((int)p2.x, (int)p2.y, (int)p2.z);

        // rotate normal
        glm::vec3 n = glm::vec3(s.normal);
        glm::vec3 n2 = glm::round(glm::vec3(q * n));
        s.normal = glm::ivec3((int)n2.x, (int)n2.y, (int)n2.z);
    }

    // rebuild base models for all stickers (positions changed)
    for (auto &s : m_stickers) {
        rebuildBaseModel(s);
    }
}

std::vector<glm::mat4> Core::getStickerModelMatrices()
{
    std::vector<glm::mat4> mats;
    mats.reserve(m_stickers.size());

    // if animating, compute an extra rotation around the proper axis & center
    bool anim = m_anim.active;
    glm::quat q_anim = glm::quat(1,0,0,0);
    glm::vec3 axisCenter(0.0f);
    if (anim) {
        float curAngle = m_anim.currentAngle;
        float rad = glm::radians(curAngle);
        glm::vec3 a = glm::normalize(m_anim.axis);
        q_anim = glm::angleAxis(rad, a);
        axisCenter = glm::vec3(m_anim.axis) * (float)m_anim.layer * m_spacing;
    }

    for (const auto &s : m_stickers) {
        if (!anim) {
            mats.push_back(s.baseModel);
            continue;
        }
        // determine if this sticker's cubePos belongs to rotating layer
        int coord = 0;
        if (std::abs(m_anim.axis.x) > 0.5f) coord = s.cubePos.x;
        else if (std::abs(m_anim.axis.y) > 0.5f) coord = s.cubePos.y;
        else coord = s.cubePos.z;

        if (coord != m_anim.layer) {
            mats.push_back(s.baseModel);
        } else {
            // final = T(center) * R * T(-center) * baseModel
            glm::mat4 T1 = glm::translate(glm::mat4(1.0f), axisCenter);
            glm::mat4 R = glm::toMat4(q_anim);
            glm::mat4 T2 = glm::translate(glm::mat4(1.0f), -axisCenter);
            mats.push_back(T1 * R * T2 * s.baseModel);
        }
    }
    return mats;
}

std::vector<glm::vec3> Core::getStickerColors()
{
    std::vector<glm::vec3> cols;
    cols.reserve(m_stickers.size());
    for (const auto &s : m_stickers) cols.push_back(s.color);
    return cols;
}