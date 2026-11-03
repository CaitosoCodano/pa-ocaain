#pragma once
#include <algorithm>
#include <cmath>

namespace ocaain {
constexpr float Degrees = 57.29577951308232f;
struct Vec { float x{}, y{}, z{}; };
inline float wrap(float value) { return std::remainder(value, 360.0f); }
inline bool finite(Vec v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline bool anglesTo(Vec origin, Vec head, Vec& out) {
    if (!finite(origin) || !finite(head)) return false;
    const float x = head.x-origin.x, y = head.y-origin.y, z = head.z-origin.z;
    const float horizontal = std::hypot(x, y);
    if (horizontal < 0.001f && std::abs(z) < 0.001f) return false;
    out = {std::clamp(-std::atan2(z, horizontal)*Degrees, -89.0f, 89.0f),
           wrap(std::atan2(y, x)*Degrees), 0};
    return finite(out);
}
inline float angularDistance(Vec from, Vec to) {
    return std::hypot(wrap(to.x-from.x), wrap(to.y-from.y));
}
inline void correctMovement(float oldYaw, float newYaw, float& forward, float& side) {
    const float delta = wrap(newYaw-oldYaw) / Degrees;
    const float f = forward, s = side;
    forward = std::cos(delta)*f - std::sin(delta)*s;
    side = std::sin(delta)*f + std::cos(delta)*s;
}
inline bool enemy(int localTeam, int playerTeam, int localId, int playerId,
                  bool alive, bool fresh) {
    return localId != playerId && (localTeam == 1 || localTeam == 2)
        && (playerTeam == 1 || playerTeam == 2) && localTeam != playerTeam
        && alive && fresh;
}
}
