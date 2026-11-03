#include "game_window.h"
#include <cstdio>

namespace {
HWND create(const char* name,DWORD extra=0) {
    WNDCLASSA type{};
    type.hInstance=GetModuleHandleA(nullptr);
    type.lpfnWndProc=DefWindowProcA;
    type.lpszClassName=name;
    if(!RegisterClassA(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return nullptr;
    HWND window=CreateWindowExA(extra,name,"Ocaain regression fixture",WS_OVERLAPPEDWINDOW,
        -32000,-32000,640,480,nullptr,nullptr,type.hInstance,nullptr);
    if(window) ShowWindow(window,SW_SHOWNOACTIVATE);
    return window;
}
bool selects(const char* name) {
    HWND window=create(name);
    const bool result=window && ocaain::findGameWindow()==window;
    if(window) DestroyWindow(window);
    return result;
}
}
int main() {
    // Visible but offscreen, with no activation and no running game required.
    HWND tool=create("Ocaain.ToolFixture",WS_EX_TOOLWINDOW);
    if(!tool) { std::fputs("Could not create window fixture\n",stderr); return 1; }
    const bool sdl=selects("SDL_app");
    const bool legacy=selects("Valve001");
    const bool other=selects("Ocaain.OtherEngineClass");
    DestroyWindow(tool);
    if(!sdl || !legacy || !other) {
        std::fprintf(stderr,"Window selection failed: SDL=%d legacy=%d other=%d\n",sdl,legacy,other);
        return 1;
    }
    std::puts("SDL, legacy and class-independent window selection passed; tool window excluded");
}
