#include "bridge.h"
#include "protocol.h"
#include "aim_math.h"
#include "menu_input.h"
#include "game_window.h"
#include <GL/gl.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <limits>
#include <cstdio>
#include <cstdarg>

extern "C" void IN_ActivateMouse();
extern "C" void IN_DeactivateMouse();
extern "C" void IN_ClearStates();

namespace bridge {
cl_enginefunc_t engine{};
namespace {
using namespace ocaain;
Shared* shared{};
HANDLE mapping{}, pixelMutex{}, uiProcess{};
HWND window{};
WNDPROC previousProc{};
engine_studio_api_t originalStudio{};
r_studio_interface_t originalInterface{}, hookedInterface{};
studiohdr_t* header{};
int drawingPlayer{};
GLuint texture{};
std::int32_t uploadedSequence{-1};
bool open{}, connected{}, canAim{};
InsertKey insertState;
DWORD lastDrawTick{};
bool lastUiAlive{};
Vec eye{}, punch{};
int screenW{}, screenH{};
int menuX{}, menuY{}, menuW{MenuWidth}, menuH{MenuHeight};
std::array<int,33> teams{};
std::array<bool,33> dead{};
pfnUserMsgHook teamCallback{}, scoreCallback{}, deathCallback{};
struct Sample {
    DWORD tick{};
    bool valid{};
    Vec head{}, origin{};
    int count{};
    std::array<Vec,MAXSTUDIOBONES> bones{};
    std::array<int,MAXSTUDIOBONES> parents{};
};
std::array<Sample,33> samples{};

void diagnostic(const char* format,...) {
    char message[1024]{};
    va_list args; va_start(args,format);
    std::vsnprintf(message,sizeof(message),format,args);
    va_end(args);
    if(engine.pfnConsolePrint) engine.pfnConsolePrint(message);
    wchar_t directory[MAX_PATH]{};
    const DWORD length=GetTempPathW(MAX_PATH,directory);
    if(!length || length>=MAX_PATH) return;
    const std::wstring path=std::wstring(directory)+L"Ocaain.Native-"+
        std::to_wstring(GetCurrentProcessId())+L".log";
    HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
        nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return;
    DWORD written{};
    WriteFile(file,message,static_cast<DWORD>(std::strlen(message)),&written,nullptr);
    CloseHandle(file);
}

LONG read(volatile std::int32_t& value) {
    return InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&value),0,0);
}
void write(volatile std::int32_t& value,LONG data) {
    InterlockedExchange(reinterpret_cast<volatile LONG*>(&value),data);
}
bool uiAlive() {
    return shared && read(shared->uiReady) &&
        static_cast<DWORD>(GetTickCount()-static_cast<DWORD>(read(shared->uiTick))) < 1500;
}
int features() { return uiAlive()?read(shared->features):0; }
void status() {
    DWORD exitCode=STILL_ACTIVE;
    const bool process=uiProcess && GetExitCodeProcess(uiProcess,&exitCode);
    char name[128]{};
    if(window) GetClassNameA(window,name,sizeof(name));
    diagnostic("Ocaain v0.2-menu-fix: map=%d window=%d class=%s hook=%d ui_ready=%d menu=%d render_seen=%d render_age_ms=%lu\n",
        connected,window!=nullptr,name,previousProc!=nullptr,uiAlive(),open,
        lastDrawTick!=0,
        static_cast<unsigned long>(lastDrawTick?GetTickCount()-lastDrawTick:0));
    if(process) diagnostic("Ocaain: menu_process=%s exit_code=%lu\n",
        exitCode==STILL_ACTIVE?"running":"stopped",static_cast<unsigned long>(exitCode));
    else diagnostic("Ocaain: menu_process=not_started\n");
}
void setOpen(bool value) {
    if(open==value) return;
    open=value;
    if(shared) write(shared->menuOpen,value);
    IN_ClearStates();
    if(value) {
        static char release[]="-forward;-back;-moveleft;-moveright;-left;-right;-lookup;-lookdown;-attack;-attack2;-jump;-duck;-use;-speed;-strafe;-mlook;-klook;-moveup;-movedown";
        engine.pfnClientCmd(release);
        IN_DeactivateMouse();
        POINT pt{menuX+menuW/2,menuY+menuH/2};
        ClientToScreen(window,&pt);
        SetCursorPos(pt.x,pt.y);
    } else IN_ActivateMouse();
}
bool freshEnemy(int id) {
    const auto* local=engine.GetLocalPlayer();
    if(!local || local->index<1 || local->index>32 || id<1 || id>32) return false;
    const auto* entity=engine.GetEntityByIndex(id);
    const auto& sample=samples[id];
    return enemy(teams[local->index],teams[id],local->index,id,
        entity && entity->player && entity->curstate.solid && !dead[id],
        sample.valid && GetTickCount()-sample.tick < 150 && entity &&
        entity->curstate.messagenum==local->curstate.messagenum);
}
int teamMessage(const char* name,int size,void* data) {
    if(data && size>=3) {
        const auto* bytes=static_cast<const unsigned char*>(data);
        const auto* text=reinterpret_cast<const char*>(bytes+1);
        const auto* end=static_cast<const char*>(std::memchr(text,0,size-1));
        if(end && bytes[0]>=1 && bytes[0]<=32) {
            const std::string team(text,end);
            teams[bytes[0]]=team=="TERRORIST"?1:team=="CT"?2:0;
            if(teams[bytes[0]]==0) samples[bytes[0]].valid=false;
        }
    }
    return teamCallback?teamCallback(name,size,data):0;
}
int scoreMessage(const char* name,int size,void* data) {
    if(data && size>=2) {
        const auto* b=static_cast<const unsigned char*>(data);
        if(b[0]>=1 && b[0]<=32) {
            dead[b[0]]=(b[1]&1)!=0;
            if(dead[b[0]]) samples[b[0]].valid=false;
        }
    }
    return scoreCallback?scoreCallback(name,size,data):0;
}
int deathMessage(const char* name,int size,void* data) {
    if(data && size>=2) {
        const auto* b=static_cast<const unsigned char*>(data);
        if(b[1]>=1 && b[1]<=32) { dead[b[1]]=true; samples[b[1]].valid=false; }
    }
    return deathCallback?deathCallback(name,size,data):0;
}

Vec transform(const float m[3][4],Vec v) {
    return {m[0][0]*v.x+m[0][1]*v.y+m[0][2]*v.z+m[0][3],
            m[1][0]*v.x+m[1][1]*v.y+m[1][2]*v.z+m[1][3],
            m[2][0]*v.x+m[2][1]*v.y+m[2][2]*v.z+m[2][3]};
}
bool range(int offset,int count,std::size_t item) {
    return header && offset>=static_cast<int>(sizeof(studiohdr_t)) && count>0 &&
        static_cast<std::size_t>(offset)+static_cast<std::size_t>(count)*item<=
        static_cast<std::size_t>(header->length);
}
void capture() {
    if(drawingPlayer<1 || drawingPlayer>32 || !header || header->length<=0 ||
       header->numbones<1 || header->numbones>MAXSTUDIOBONES ||
       !range(header->boneindex,header->numbones,sizeof(mstudiobone_t)) ||
       !range(header->hitboxindex,header->numhitboxes,sizeof(mstudiobbox_t))) return;
    auto* entity=originalStudio.GetCurrentEntity();
    if(!entity || !entity->player || entity->index!=drawingPlayer) return;
    const auto* base=reinterpret_cast<const unsigned char*>(header);
    const auto* boxes=reinterpret_cast<const mstudiobbox_t*>(base+header->hitboxindex);
    const mstudiobbox_t* headBox{};
    for(int i=0;i<header->numhitboxes;++i) {
        if(boxes[i].group==1 && boxes[i].bone>=0 && boxes[i].bone<header->numbones) {
            headBox=&boxes[i]; break;
        }
    }
    // Weapon models and custom models without a head hitbox are not guessed.
    if(!headBox) return;
    const auto* bones=reinterpret_cast<const mstudiobone_t*>(base+header->boneindex);
    auto* transforms=reinterpret_cast<float(*)[3][4]>(originalStudio.StudioGetBoneTransform());
    if(!transforms) return;
    auto& sample=samples[drawingPlayer];
    sample.valid=false;
    sample.count=header->numbones;
    for(int i=0;i<sample.count;++i) {
        sample.bones[i]={transforms[i][0][3],transforms[i][1][3],transforms[i][2][3]};
        sample.parents[i]=bones[i].parent;
        if(!finite(sample.bones[i])) return;
    }
    Vec center{(headBox->bbmin[0]+headBox->bbmax[0])*0.5f,
               (headBox->bbmin[1]+headBox->bbmax[1])*0.5f,
               (headBox->bbmin[2]+headBox->bbmax[2])*0.5f};
    sample.head=transform(transforms[headBox->bone],center);
    sample.origin={entity->origin[0],entity->origin[1],entity->origin[2]};
    sample.tick=GetTickCount();
    sample.valid=finite(sample.head);
}
void setHeader(void* value) {
    header=static_cast<studiohdr_t*>(value);
    originalStudio.StudioSetHeader(value);
}
void setupModel(int body,void** part,void** submodel) {
    capture(); // Bones have been computed; weapon rendering has not replaced them.
    originalStudio.StudioSetupModel(body,part,submodel);
}
int drawPlayer(int flags,entity_state_t* state) {
    drawingPlayer=state?state->number:0;
    const int result=originalInterface.StudioDrawPlayer(flags,state);
    drawingPlayer=0;
    return result;
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_KEYDOWN && w==VK_INSERT) {
        insertKey(true);
        return 0;
    }
    if(msg==WM_KEYUP && w==VK_INSERT) { insertKey(false); return 0; }
    if(msg==WM_KILLFOCUS || (msg==WM_ACTIVATEAPP && !w)) {
        insertState.reset();
        if(open) setOpen(false);
    }
    if(open) {
        if(msg==WM_LBUTTONDOWN && shared) {
            POINT p{}; GetCursorPos(&p); ScreenToClient(hwnd,&p);
            RECT rect{}; GetClientRect(hwnd,&rect);
            if(rect.right>0 && rect.bottom>0 && menuW>0 && menuH>0) {
                const int x=p.x*screenW/rect.right-menuX;
                const int y=p.y*screenH/rect.bottom-menuY;
                write(shared->clickX,x*MenuWidth/menuW);
                write(shared->clickY,y*MenuHeight/menuH);
                InterlockedIncrement(reinterpret_cast<volatile LONG*>(&shared->clickSequence));
            }
            return 0;
        }
        if((msg>=WM_MOUSEFIRST && msg<=WM_MOUSELAST) || msg==WM_INPUT ||
           (msg>=WM_KEYFIRST && msg<=WM_KEYLAST)) return 0;
    }
    return CallWindowProcW(previousProc,hwnd,msg,w,l);
}
void ensureWindow() {
    if(window && previousProc && IsWindow(window)) return;
    window=nullptr;
    previousProc=nullptr;
    HWND candidate=findGameWindow();
    if(!candidate) return;
    SetLastError(0);
    auto previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(candidate,GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(windowProc)));
    if(!previous) {
        const DWORD error=GetLastError();
        static DWORD lastError{}, lastReport{};
        if(error!=lastError || GetTickCount()-lastReport>2000) {
            diagnostic("Ocaain: window_hook_failed error=%lu\n",static_cast<unsigned long>(error));
            lastError=error; lastReport=GetTickCount();
        }
        return;
    }
    window=candidate;
    previousProc=previous;
    char name[128]{}; GetClassNameA(window,name,sizeof(name));
    diagnostic("Ocaain: game_window_hooked class=%s\n",name);
}
void launchMenu() {
    HMODULE self{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&launchMenu),&self);
    wchar_t path[32768]{};
    GetModuleFileNameW(self,path,32768);
    std::wstring exe(path);
    exe.resize(exe.find_last_of(L"\\/")+1);
    exe+=L"ocaain-menu\\Ocaain.Menu.exe";
    std::wstring args=L"\""+exe+L"\" --pid "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW start{};
    start.cb=sizeof(start);
    PROCESS_INFORMATION process{};
    if(CreateProcessW(exe.c_str(),args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,
        nullptr,nullptr,&start,&process)) {
        uiProcess=process.hProcess;
        CloseHandle(process.hThread);
        diagnostic("Ocaain: menu_process_started pid=%lu\n",static_cast<unsigned long>(process.dwProcessId));
    } else diagnostic("Ocaain: menu_process_start_failed error=%lu\n",static_cast<unsigned long>(GetLastError()));
}
bool project(Vec world,float& x,float& y) {
    float point[3]{world.x,world.y,world.z}, result[3]{};
    if(!finite(world) || engine.pTriAPI->WorldToScreen(point,result)!=0 ||
       !std::isfinite(result[0]) || !std::isfinite(result[1])) return false;
    x=(result[0]+1)*0.5f*screenW;
    y=(1-result[1])*0.5f*screenH;
    return std::abs(x)<screenW*8 && std::abs(y)<screenH*8;
}
void line(float x1,float y1,float x2,float y2) {
    glBegin(GL_LINES); glVertex2f(x1,y1); glVertex2f(x2,y2); glEnd();
}
void esp(int flags) {
    int count=0;
    for(int id=1;id<=std::min(32,engine.GetMaxClients());++id) {
        if(!freshEnemy(id)) continue;
        ++count;
        if(!(flags&(Boxes|Skeleton))) continue;
        const auto& s=samples[id];
        float minX=std::numeric_limits<float>::max(), minY=minX;
        float maxX=-minX,maxY=-minX;
        int projected=0;
        for(int b=0;b<s.count;++b) {
            float x{},y{};
            if(!project(s.bones[b],x,y)) continue;
            ++projected;
            minX=std::min(minX,x); maxX=std::max(maxX,x);
            minY=std::min(minY,y); maxY=std::max(maxY,y);
            const int parent=s.parents[b];
            if((flags&Skeleton) && parent>=0 && parent<s.count) {
                float px{},py{};
                if(project(s.bones[parent],px,py)) {
                    glColor4ub(70,230,205,230); line(x,y,px,py);
                }
            }
        }
        if((flags&Boxes) && projected>=2 && maxX>minX && maxY>minY) {
            glColor4ub(255,105,95,240);
            minX-=5; maxX+=5; minY-=8; maxY+=5;
            glBegin(GL_LINE_LOOP);
            glVertex2f(minX,minY); glVertex2f(maxX,minY);
            glVertex2f(maxX,maxY); glVertex2f(minX,maxY); glEnd();
        }
    }
    if(shared) write(shared->enemies,count);
}
void drawMenu() {
    if(!uiAlive()) return;
    if(!glIsTexture(texture)) {
        glGenTextures(1,&texture);
        glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,MenuWidth,MenuHeight,0,0x80E1,GL_UNSIGNED_BYTE,nullptr);
        uploadedSequence=-1;
    }
    glBindTexture(GL_TEXTURE_2D,texture);
    const DWORD wait=WaitForSingleObject(pixelMutex,0);
    if(wait==WAIT_OBJECT_0 || wait==WAIT_ABANDONED) {
        const LONG sequence=read(shared->pixelSequence);
        if(sequence!=uploadedSequence) {
            glPixelStorei(GL_UNPACK_ALIGNMENT,4);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,MenuWidth,MenuHeight,0x80E1,GL_UNSIGNED_BYTE,shared->pixels);
            uploadedSequence=sequence;
        }
        ReleaseMutex(pixelMutex);
    }
    if(uploadedSequence<0) return;
    const float scale=std::min({1.0f,screenW/static_cast<float>(MenuWidth),screenH/static_cast<float>(MenuHeight)});
    menuW=static_cast<int>(MenuWidth*scale); menuH=static_cast<int>(MenuHeight*scale);
    menuX=(screenW-menuW)/2; menuY=(screenH-menuH)/2;
    glEnable(GL_TEXTURE_2D); glColor4ub(255,255,255,255);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2i(menuX,menuY);
    glTexCoord2f(1,0); glVertex2i(menuX+menuW,menuY);
    glTexCoord2f(1,1); glVertex2i(menuX+menuW,menuY+menuH);
    glTexCoord2f(0,1); glVertex2i(menuX,menuY+menuH);
    glEnd(); glDisable(GL_TEXTURE_2D);
    POINT p{}; RECT r{}; GetCursorPos(&p); ScreenToClient(window,&p); GetClientRect(window,&r);
    if(r.right>0 && r.bottom>0) {
        const float x=p.x*static_cast<float>(screenW)/r.right;
        const float y=p.y*static_cast<float>(screenH)/r.bottom;
        glColor4ub(255,255,255,255);
        glBegin(GL_TRIANGLES); glVertex2f(x,y); glVertex2f(x+3,y+16); glVertex2f(x+11,y+11); glEnd();
    }
}
}

bool initialize() {
    diagnostic("Ocaain v0.2-menu-fix: client_loaded\n");
    static char command[]="ocaain_status";
    engine.pfnAddCommand(command,status);
    const auto id=std::to_wstring(GetCurrentProcessId());
    const std::wstring name=L"Local\\Ocaain_"+id;
    mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),name.c_str());
    if(!mapping || GetLastError()==ERROR_ALREADY_EXISTS) {
        diagnostic("Ocaain: shared_memory_create_failed error=%lu\n",static_cast<unsigned long>(GetLastError()));
        if(mapping) CloseHandle(mapping);
        mapping=nullptr; return false;
    }
    shared=static_cast<Shared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
    if(!shared) {
        diagnostic("Ocaain: shared_memory_map_failed error=%lu\n",static_cast<unsigned long>(GetLastError()));
        CloseHandle(mapping); mapping=nullptr; return false;
    }
    std::memset(shared,0,sizeof(Shared));
    shared->magic=Magic; shared->version=1; shared->processId=GetCurrentProcessId();
    pixelMutex=CreateMutexW(nullptr,FALSE,(name+L"_Pixels").c_str());
    if(!pixelMutex) {
        diagnostic("Ocaain: pixel_mutex_create_failed error=%lu\n",static_cast<unsigned long>(GetLastError()));
        shutdown(); return false;
    }
    launchMenu();
    return true;
}
bool menuOpen() { return open; }
void insertKey(bool down) {
    if(!insertState.update(down)) return;
    ensureWindow();
    if(!connected || !window || !uiAlive()) {
        diagnostic("Ocaain: insert_received_but_not_ready\n");
        status();
        return;
    }
    setOpen(!open);
    diagnostic("Ocaain: insert_menu=%s\n",open?"open":"closed");
}
void reset() {
    samples={}; dead={}; canAim=false;
}
int hookUserMessage(char* name,pfnUserMsgHook callback) {
    if(name && std::strcmp(name,"TeamInfo")==0) { teamCallback=callback; return engine.pfnHookUserMsg(name,teamMessage); }
    if(name && std::strcmp(name,"ScoreAttrib")==0) { scoreCallback=callback; return engine.pfnHookUserMsg(name,scoreMessage); }
    if(name && std::strcmp(name,"DeathMsg")==0) { deathCallback=callback; return engine.pfnHookUserMsg(name,deathMessage); }
    return engine.pfnHookUserMsg(name,callback);
}
void studio(engine_studio_api_t* api,r_studio_interface_t** iface) {
    if(api) {
        originalStudio=*api;
        api->StudioSetHeader=setHeader;
        api->StudioSetupModel=setupModel;
    }
    if(iface && *iface) {
        originalInterface=**iface;
        hookedInterface=originalInterface;
        hookedInterface.StudioDrawPlayer=drawPlayer;
        *iface=&hookedInterface;
    }
}
void frame() {
    ensureWindow();
    const char* level=engine.pfnGetLevelName();
    const bool wasConnected=connected;
    connected=level && *level && engine.GetLocalPlayer();
    if(!connected && wasConnected) { reset(); teams={}; }
    if(shared) write(shared->gameTick,static_cast<LONG>(GetTickCount()));
    const bool alive=uiAlive();
    if(alive!=lastUiAlive) {
        diagnostic("Ocaain: menu_renderer=%s\n",alive?"ready":"not_ready");
        lastUiAlive=alive;
    }
    if(open && (!connected || !uiAlive() || GetForegroundWindow()!=window)) setOpen(false);
    if(!uiAlive() && shared) write(shared->features,0);
}
void view(ref_params_t* p) {
    if(!p) return;
    eye={p->vieworg[0],p->vieworg[1],p->vieworg[2]};
    punch={p->punchangle[0],p->punchangle[1],p->punchangle[2]};
    canAim=p->health>0 && !p->spectator && !p->intermission && !p->demoplayback;
}
void move(usercmd_t* cmd,int active) {
    if(!cmd) return;
    if(shared) write(shared->target,0);
    if(open) { cmd->buttons=0; cmd->forwardmove=cmd->sidemove=cmd->upmove=0; return; }
    if(!active || !canAim || !connected || !window || GetForegroundWindow()!=window || !(features()&Aim)) return;
    auto* local=engine.GetLocalPlayer();
    if(!local || local->index<1 || local->index>32 || dead[local->index]) return;
    Vec best{}; int target=0;
    float distance=std::numeric_limits<float>::max();
    for(int id=1;id<=std::min(32,engine.GetMaxClients());++id) {
        if(!freshEnemy(id)) continue;
        Vec angle{};
        if(!anglesTo(eye,samples[id].head,angle)) continue;
        const float score=angularDistance({cmd->viewangles[0],cmd->viewangles[1],0},angle);
        if(score<distance) { distance=score; best=angle; target=id; }
    }
    if(!target) return;
    // GoldSrc adds punch angles to the rendered view. Counter this component.
    best.x=std::clamp(best.x-punch.x,-89.0f,89.0f);
    best.y=wrap(best.y-punch.y);
    correctMovement(cmd->viewangles[1],best.y,cmd->forwardmove,cmd->sidemove);
    cmd->viewangles[0]=best.x; cmd->viewangles[1]=best.y; cmd->viewangles[2]=0;
    float angles[3]{best.x,best.y,0}; engine.SetViewAngles(angles);
    if(shared) write(shared->target,target);
}
void draw() {
    ensureWindow();
    if(wglGetCurrentContext()) lastDrawTick=GetTickCount();
    if(!connected || !window || GetForegroundWindow()!=window || !engine.pTriAPI || !wglGetCurrentContext()) return;
    SCREENINFO info{}; info.iSize=sizeof(info);
    if(!engine.pfnGetScreenInfo(&info) || info.iWidth<=0 || info.iHeight<=0) return;
    screenW=info.iWidth; screenH=info.iHeight;
    const int flags=features();
    if(!flags && !open) return;
    GLint oldMode{}; glGetIntegerv(GL_MATRIX_MODE,&oldMode);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glViewport(0,0,screenW,screenH);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0,screenW,screenH,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST); glDisable(GL_SCISSOR_TEST); glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glLineWidth(1.5f);
    esp(flags);
    if(open) drawMenu();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(oldMode); glPopAttrib();
}
void shutdown() {
    if(open) setOpen(false);
    if(window && previousProc && IsWindow(window) &&
       reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC))==windowProc)
        SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(previousProc));
    if(shared) { write(shared->features,0); write(shared->shutdown,1); }
    if(uiProcess) { WaitForSingleObject(uiProcess,500); CloseHandle(uiProcess); uiProcess=nullptr; }
    if(texture && wglGetCurrentContext()) glDeleteTextures(1,&texture);
    texture=0;
    if(shared) { UnmapViewOfFile(shared); shared=nullptr; }
    if(pixelMutex) { CloseHandle(pixelMutex); pixelMutex=nullptr; }
    if(mapping) { CloseHandle(mapping); mapping=nullptr; }
    previousProc=nullptr; window=nullptr;
}
}
