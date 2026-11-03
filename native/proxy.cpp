#include "bridge.h"
#include <string>

namespace {
HMODULE original;
cldll_func_t real{};
bool loadOriginal() {
    if (original) return true;
    HMODULE self{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&loadOriginal), &self);
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(self, path, 32768)) return false;
    std::wstring file(path);
    file.resize(file.find_last_of(L"\\/")+1);
    file += L"client.original.dll";
    original = LoadLibraryExW(file.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!original) MessageBoxW(nullptr,
        L"Não foi possível carregar client.original.dll. Restaure a instalação ou execute Install.ps1 com o jogo fechado.",
        L"Ocaain", MB_ICONERROR);
    return original != nullptr;
}
template<typename T> T get(const char* name, T known) {
    if (!loadOriginal()) return nullptr;
    return known ? known : reinterpret_cast<T>(GetProcAddress(original,name));
}
}

#define API extern "C"
#define VOID_FORWARD(name, type, field, decl, args) \
    API void name decl { auto fn=get<type>(#name,real.field); if(fn) fn args; }
#define RET_FORWARD(ret, name, type, field, decl, args, fallback) \
    API ret name decl { auto fn=get<type>(#name,real.field); return fn ? fn args : fallback; }

API int Initialize(cl_enginefunc_t* e, int version) {
    if (!e || version != CLDLL_INTERFACE_VERSION) return 0;
    auto fn=get<INITIALIZE_FUNC>("Initialize",real.pInitFunc);
    if (!fn) return 0;
    bridge::engine=*e;
    // Fail closed; no attempt to bypass VAC or secure-client validation.
    char insecure[]="-insecure";
    char* next{};
    if (!e->CheckParm(insecure,&next)) {
        MessageBoxW(nullptr,L"Inicie o CS 1.6 com -insecure para usar este cliente local. Restaure a DLL original antes de jogar em servidores protegidos.",L"Ocaain",MB_ICONERROR);
        return 0;
    }
    static cl_enginefunc_t hooked;
    hooked=*e;
    hooked.pfnHookUserMsg=bridge::hookUserMessage;
    const int ok=fn(&hooked,version);
    if (ok) bridge::initialize();
    return ok;
}
API int HUD_Redraw(float time,int intermission) {
    auto fn=get<HUD_REDRAW_FUNC>("HUD_Redraw",real.pHudRedrawFunc);
    const int result=fn?fn(time,intermission):0;
    bridge::draw();
    return result;
}
API void HUD_Frame(double time) {
    auto fn=get<HUD_FRAME_FUNC>("HUD_Frame",real.pHudFrame);
    if(fn) fn(time);
    bridge::frame();
}
API void CL_CreateMove(float dt,usercmd_t* cmd,int active) {
    auto fn=get<HUD_CL_CREATEMOVE_FUNC>("CL_CreateMove",real.pCL_CreateMove);
    if(fn) fn(dt,cmd,active);
    bridge::move(cmd,active);
}
API void V_CalcRefdef(ref_params_t* params) {
    auto fn=get<HUD_CALCREF_FUNC>("V_CalcRefdef",real.pCalcRefdef);
    if(fn) fn(params);
    bridge::view(params);
}
API int HUD_GetStudioModelInterface(int version,r_studio_interface_t** iface,engine_studio_api_t* api) {
    auto fn=get<HUD_STUDIO_INTERFACE_FUNC>("HUD_GetStudioModelInterface",real.pStudioInterface);
    static engine_studio_api_t hooked;
    if(!fn || !api || !iface) return 0;
    hooked=*api;
    bridge::studio(&hooked,nullptr);
    const int result=fn(version,iface,&hooked);
    if(result && *iface) bridge::studio(nullptr,iface);
    return result;
}
API void HUD_Reset() {
    bridge::reset();
    auto fn=get<HUD_RESET_FUNC>("HUD_Reset",real.pHudResetFunc);
    if(fn) fn();
}
API void HUD_Shutdown() {
    bridge::shutdown();
    auto fn=get<HUD_SHUTDOWN_FUNC>("HUD_Shutdown",real.pShutdown);
    if(fn) fn();
}
API int HUD_Key_Event(int down,int key,const char* binding) {
    if(key==K_INS) { bridge::insertKey(down!=0); return 0; }
    if(bridge::menuOpen()) return 0;
    auto fn=get<HUD_KEY_EVENT_FUNC>("HUD_Key_Event",real.pKeyEvent);
    return fn?fn(down,key,binding):1;
}
VOID_FORWARD(HUD_Init,HUD_INIT_FUNC,pHudInitFunc,(),())
RET_FORWARD(int,HUD_VidInit,HUD_VIDINIT_FUNC,pHudVidInitFunc,(),(),0)
RET_FORWARD(int,HUD_UpdateClientData,HUD_UPDATECLIENTDATA_FUNC,pHudUpdateClientDataFunc,(client_data_t* d,float t),(d,t),0)
VOID_FORWARD(HUD_PlayerMove,HUD_CLIENTMOVE_FUNC,pClientMove,(playermove_s* p,qboolean server),(p,server))
VOID_FORWARD(HUD_PlayerMoveInit,HUD_CLIENTMOVEINIT_FUNC,pClientMoveInit,(playermove_s* p),(p))
RET_FORWARD(char,HUD_PlayerMoveTexture,HUD_TEXTURETYPE_FUNC,pClientTextureType,(char* n),(n),0)
VOID_FORWARD(IN_ActivateMouse,HUD_IN_ACTIVATEMOUSE_FUNC,pIN_ActivateMouse,(),())
VOID_FORWARD(IN_DeactivateMouse,HUD_IN_DEACTIVATEMOUSE_FUNC,pIN_DeactivateMouse,(),())
VOID_FORWARD(IN_MouseEvent,HUD_IN_MOUSEEVENT_FUNC,pIN_MouseEvent,(int m),(m))
VOID_FORWARD(IN_ClearStates,HUD_IN_CLEARSTATES_FUNC,pIN_ClearStates,(),())
VOID_FORWARD(IN_Accumulate,HUD_IN_ACCUMULATE_FUNC,pIN_Accumulate,(),())
RET_FORWARD(int,CL_IsThirdPerson,HUD_CL_ISTHIRDPERSON_FUNC,pCL_IsThirdPerson,(),(),0)
VOID_FORWARD(CL_CameraOffset,HUD_CL_GETCAMERAOFFSETS_FUNC,pCL_GetCameraOffsets,(float* o),(o))
RET_FORWARD(kbutton_s*,KB_Find,HUD_KB_FIND_FUNC,pFindKey,(const char* n),(n),nullptr)
VOID_FORWARD(CAM_Think,HUD_CAMTHINK_FUNC,pCamThink,(),())
RET_FORWARD(int,HUD_AddEntity,HUD_ADDENTITY_FUNC,pAddEntity,(int t,cl_entity_t* e,const char* n),(t,e,n),1)
VOID_FORWARD(HUD_CreateEntities,HUD_CREATEENTITIES_FUNC,pCreateEntities,(),())
VOID_FORWARD(HUD_DrawNormalTriangles,HUD_DRAWNORMALTRIS_FUNC,pDrawNormalTriangles,(),())
VOID_FORWARD(HUD_DrawTransparentTriangles,HUD_DRAWTRANSTRIS_FUNC,pDrawTransparentTriangles,(),())
VOID_FORWARD(HUD_StudioEvent,HUD_STUDIOEVENT_FUNC,pStudioEvent,(const mstudioevent_s* ev,const cl_entity_t* e),(ev,e))
VOID_FORWARD(HUD_PostRunCmd,HUD_POSTRUNCMD_FUNC,pPostRunCmd,(local_state_s* a,local_state_s* b,usercmd_s* c,int r,double t,unsigned int seed),(a,b,c,r,t,seed))
VOID_FORWARD(HUD_TxferLocalOverrides,HUD_TXFERLOCALOVERRIDES_FUNC,pTxferLocalOverrides,(entity_state_s* s,const clientdata_s* c),(s,c))
VOID_FORWARD(HUD_ProcessPlayerState,HUD_PROCESSPLAYERSTATE_FUNC,pProcessPlayerState,(entity_state_s* d,const entity_state_s* s),(d,s))
VOID_FORWARD(HUD_TxferPredictionData,HUD_TXFERPREDICTIONDATA_FUNC,pTxferPredictionData,(entity_state_s* a,const entity_state_s* b,clientdata_s* c,const clientdata_s* d,weapon_data_s* e,const weapon_data_s* f),(a,b,c,d,e,f))
VOID_FORWARD(Demo_ReadBuffer,HUD_DEMOREAD_FUNC,pReadDemoBuffer,(int s,unsigned char* b),(s,b))
RET_FORWARD(int,HUD_ConnectionlessPacket,HUD_CONNECTIONLESS_FUNC,pConnectionlessPacket,(const netadr_s* n,const char* a,char* b,int* s),(n,a,b,s),0)
RET_FORWARD(int,HUD_GetHullBounds,HUD_GETHULLBOUNDS_FUNC,pGetHullBounds,(int h,float* a,float* b),(h,a,b),0)
VOID_FORWARD(HUD_TempEntUpdate,HUD_TEMPENTUPDATE_FUNC,pTempEntUpdate,(double a,double b,double c,tempent_s** d,tempent_s** e,int(*f)(cl_entity_s*),void(*g)(tempent_s*,float)),(a,b,c,d,e,f,g))
RET_FORWARD(cl_entity_s*,HUD_GetUserEntity,HUD_GETUSERENTITY_FUNC,pGetUserEntity,(int i),(i),nullptr)
VOID_FORWARD(HUD_VoiceStatus,HUD_VOICESTATUS_FUNC,pVoiceStatus,(int i,qboolean b),(i,b))
VOID_FORWARD(HUD_DirectorMessage,HUD_DIRECTORMESSAGE_FUNC,pDirectorMessage,(int s,void* b),(s,b))
VOID_FORWARD(HUD_ChatInputPosition,HUD_CHATINPUTPOSITION_FUNC,pChatInputPosition,(int* x,int* y),(x,y))
RET_FORWARD(int,HUD_GetPlayerTeam,HUD_GETPLAYERTEAM,pGetPlayerTeam,(int i),(i),0)
RET_FORWARD(void*,ClientFactory,CLIENTFACTORY,pClientFactory,(),(),nullptr)
API void* CreateInterface(const char* name,int* code) {
    using Fn=void*(*)(const char*,int*);
    auto fn=get<Fn>("CreateInterface",nullptr);
    return fn?fn(name,code):nullptr;
}
API void F(void* table) {
    using Fn=void(*)(void*);
    auto fn=get<Fn>("F",nullptr);
    if(!fn || !table) return;
    fn(table);
    real=*static_cast<cldll_func_t*>(table);
    auto* t=static_cast<cldll_func_t*>(table);
    t->pInitFunc=Initialize;
    t->pHudRedrawFunc=HUD_Redraw;
    t->pHudFrame=HUD_Frame;
    t->pCL_CreateMove=CL_CreateMove;
    t->pCalcRefdef=V_CalcRefdef;
    t->pStudioInterface=HUD_GetStudioModelInterface;
    t->pHudResetFunc=HUD_Reset;
    t->pShutdown=HUD_Shutdown;
    t->pKeyEvent=HUD_Key_Event;
}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID) { return TRUE; }
