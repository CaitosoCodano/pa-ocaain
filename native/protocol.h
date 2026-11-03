#pragma once
#include <cstdint>
#include <cstddef>

namespace ocaain {
constexpr std::uint32_t Magic = 0x3141434f;
constexpr int MenuWidth = 480, MenuHeight = 380;
enum Feature : std::uint32_t { Aim = 1, Boxes = 2, Skeleton = 4 };
// Fixed-width, naturally aligned fields. One writer per field; pixels protected
// by the named mutex, commands/heartbeats exchanged with interlocked operations.
struct Shared {
    std::uint32_t magic, version, processId, reserved;
    volatile std::int32_t features, menuOpen, gameTick, uiTick;
    volatile std::int32_t clickSequence, clickX, clickY, pixelSequence;
    volatile std::int32_t enemies, target, shutdown, uiReady;
    std::uint8_t pixels[MenuWidth * MenuHeight * 4]; // BGRA, top down
};
static_assert(offsetof(Shared, pixels) == 64);
static_assert(sizeof(Shared) == 64 + MenuWidth * MenuHeight * 4);
}
