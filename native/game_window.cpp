#include "game_window.h"
#include <GL/gl.h>
#include <cstdint>
#include <cstring>

namespace ocaain {
namespace {
bool gameCandidate(HWND hwnd) {
    if(!hwnd || !IsWindowVisible(hwnd) || GetWindow(hwnd,GW_OWNER)) return false;
    DWORD pid{};
    GetWindowThreadProcessId(hwnd,&pid);
    RECT rect{};
    return pid==GetCurrentProcessId() && GetClientRect(hwnd,&rect) &&
        rect.right>=128 && rect.bottom>=128 &&
        !(GetWindowLongPtrW(hwnd,GWL_EXSTYLE)&WS_EX_TOOLWINDOW);
}
struct Search {
    HWND foreground{}, best{};
    std::int64_t score{-1};
};
BOOL CALLBACK enumerate(HWND hwnd,LPARAM context) {
    if(!gameCandidate(hwnd)) return TRUE;
    auto& search=*reinterpret_cast<Search*>(context);
    RECT rect{}; GetClientRect(hwnd,&rect);
    char name[128]{}; GetClassNameA(hwnd,name,sizeof(name));
    const bool known=std::strcmp(name,"Valve001")==0 || std::strcmp(name,"SDL_app")==0;
    const std::int64_t score=static_cast<std::int64_t>(rect.right)*rect.bottom +
        (known?1LL<<40:0) + (hwnd==search.foreground?1LL<<41:0);
    if(score>search.score) { search.score=score; search.best=hwnd; }
    return TRUE;
}
}
HWND findGameWindow() {
    // The OpenGL drawable identifies the render window without a class-name
    // assumption. Root resolves engines that render into a child window.
    if(wglGetCurrentContext()) {
        HWND drawable=WindowFromDC(wglGetCurrentDC());
        HWND root=drawable?GetAncestor(drawable,GA_ROOT):nullptr;
        if(gameCandidate(root)) return root;
    }
    Search search{};
    search.foreground=GetForegroundWindow();
    EnumWindows(enumerate,reinterpret_cast<LPARAM>(&search));
    return search.best;
}
}
