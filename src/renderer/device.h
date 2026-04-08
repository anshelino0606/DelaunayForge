#ifndef FEM_DEVICE_H
#define FEM_DEVICE_H

#include <LLGL/LLGL.h>

namespace fem {

inline LLGL::RenderSystemPtr g_device = nullptr;
inline size_t g_frame_number = 0;
inline size_t g_frame_index = 0;
    
inline LLGL::Texture* g_dummy_2d_texture = nullptr;

inline LLGL::Sampler* g_linear_clamp_sampler = nullptr;

template<typename T>
T align_to(T value, T alignment)
{
    return ((value + alignment - T(1)) / alignment) * alignment;
}

}

#endif // FEM_DEVICE_H