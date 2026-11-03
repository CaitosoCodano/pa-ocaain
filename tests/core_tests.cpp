#include "aim_math.h"
#include "protocol.h"
#include <cstdlib>
#include <iostream>
#include <limits>

int checks = 0;
void check(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
bool near(float a, float b) { return std::abs(a-b) < 0.001f; }
int main() {
    using namespace ocaain;
    Vec aim;
    check(anglesTo({}, {100,0,0}, aim) && near(aim.x,0) && near(aim.y,0), "front");
    check(anglesTo({}, {0,100,0}, aim) && near(aim.y,90), "left");
    check(anglesTo({}, {100,0,100}, aim) && near(aim.x,-45), "head above eye");
    check(anglesTo({}, {0,0,-100}, aim) && near(aim.x,89), "pitch bounds");
    check(!anglesTo({}, {}, aim), "zero distance rejected");
    check(!anglesTo({}, {std::numeric_limits<float>::quiet_NaN(),0,0}, aim), "NaN rejected");
    check(near(angularDistance({0,179,0}, {0,-179,0}),2), "yaw wrap");
    float f=200, s=0;
    correctMovement(0,90,f,s);
    check(near(f,0) && near(s,200), "world movement preserved at yaw 90");
    correctMovement(90,0,f,s);
    check(near(f,200) && near(s,0), "movement round trip");
    check(enemy(1,2,1,2,true,true), "enemy selected");
    check(!enemy(1,1,1,2,true,true), "teammate rejected");
    check(!enemy(0,2,1,2,true,true), "unknown local team rejected");
    check(!enemy(1,0,1,2,true,true), "unknown remote team rejected");
    check(!enemy(1,2,1,1,true,true), "self rejected");
    check(!enemy(1,2,1,2,false,true), "dead rejected");
    check(!enemy(1,2,1,2,true,false), "stale rejected");
    check(offsetof(Shared,pixels)==64 && sizeof(Shared)==729664, "IPC layout");
    std::cout << checks << " checks passed\n";
}
