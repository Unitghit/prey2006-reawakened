#include "../neo/game/physics/QuakeAirMove.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void require(bool condition) {
    if (!condition) { std::fputs("Quake air acceleration check failed\n", stderr); std::exit(1); }
}
int main() {
    require(PreyQuakeAirAcceleration(320, 320, .016f) == 0); // forward at speed
    require(PreyQuakeAirAcceleration(320, 0, .016f) == 30); // perpendicular strafe
    require(PreyQuakeAirAcceleration(320, 29, .016f) == 1); // projection cap
    require(PreyQuakeAirAcceleration(20, 0, .016f) == 3.2f); // partial input
    require(PreyQuakeAirAcceleration(0, -320, .016f) == 0); // released keys
    require(PreyQuakeAirAcceleration(320, 0, 0) == 0);
    const float brake = PreyQuakeAirAcceleration(320, -320, .016f);
    require(brake > 0 && brake < 320); // opposing input brakes; no speed floor
    float vx = 320, vy = 0;
    for (int i = 0; i < 100; ++i) {
        const float speed = std::sqrt(vx * vx + vy * vy);
        const float dx = -vy / speed, dy = vx / speed;
        const float gain = PreyQuakeAirAcceleration(320, vx * dx + vy * dy, .016f);
        vx += dx * gain; vy += dy * gain;
    }
    require(std::sqrt(vx * vx + vy * vy) > 400); // continuous turning gains speed
    std::puts("PASS: projection cap, partial input, braking, idle, and strafe-turn acceleration");
}
