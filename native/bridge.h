#pragma once
#include "sdk.h"

namespace bridge {
extern cl_enginefunc_t engine;
bool initialize();
void shutdown();
void reset();
void frame();
void draw();
void move(usercmd_t* cmd, int active);
void view(ref_params_t* params);
int hookUserMessage(char* name, pfnUserMsgHook callback);
void studio(engine_studio_api_t* api, r_studio_interface_t** iface);
bool menuOpen();
}
