#pragma once

namespace ocaain {
class InsertKey {
    bool held_{};
public:
    // Both the window and engine callbacks may receive keyboard events.
    // A held key must produce one transition, not one toggle per repeated event.
    bool update(bool down) {
        const bool pressed=down && !held_;
        held_=down;
        return pressed;
    }
    void reset() { held_=false; }
};
}
