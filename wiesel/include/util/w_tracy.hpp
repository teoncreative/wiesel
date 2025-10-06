//
// Created by Metehan Gezer on 10/09/2025.
//

#ifndef WIESEL_PARENT_W_TRACY_H
#define WIESEL_PARENT_W_TRACY_H

// Tracy is a profiler for C++
// https://github.com/wolfpld/tracy

#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"
#define PROFILE_FRAME_MARK() FrameMark
#define PROFILE_ZONE_SCOPED() ZoneScoped
#define PROFILE_ZONE_SCOPED_N(name) ZoneScopedN(name)
#define PROFILE_LOCKABLE(type, varname) TracyLockable(type, varname)
#define PROFILE_MESSAGE(text, size) TracyMessage(text, size)
#define PROFILE_MESSAGE_L(text) TracyMessageL(text)
#define PROFILE_ALLOC(ptr, size) TracyAlloc(ptr, size)
#define PROFILE_FREE(ptr) TracyFree(ptr)
#define PROFILE_ZONE_VALUE(value) ZoneValue(value)
#define PROFILE_GPU_ZONE(ctx, cmd, name) TracyVkZone(ctx, cmd, name)
#define PROFILE_GPU_COLLECT(ctx, cmd) TracyVkCollect(ctx, cmd)
#if TRACY_ENABLE
    #define PROFILE_THREAD(name) tracy::SetThreadName(name)
#else
    #define PROFILE_THREAD(name)
#endif

#define PROFILE_MESSAGE(text, size) TracyMessage(text, size)
#define PROFILE_MESSAGE_L(text) TracyMessageL(text)
#define PROFILE_MESSAGE_C(text, size, color) TracyMessageC(text, size, color)

#define PROFILE_PLOT(name, value) TracyPlot(name, value)
#define PROFILE_PLOT_CONFIG(name, format) TracyPlotConfig(name, format)

// TODO Memory profiling
#define PROFILE_ALLOC(ptr, size) TracyAlloc(ptr, size)
#define PROFILE_FREE(ptr) TracyFree(ptr)
#define PROFILE_SECURE_ALLOC(ptr, size) TracySecureAlloc(ptr, size)
#define PROFILE_SECURE_FREE(ptr) TracySecureFree(ptr)

// For named memory pools
#define PROFILE_ALLOC_NAMED(ptr, size, name) TracyAllocN(ptr, size, name)
#define PROFILE_FREE_NAMED(ptr, name) TracyFreeN(ptr, name)


#endif  //WIESEL_PARENT_W_TRACY_H
