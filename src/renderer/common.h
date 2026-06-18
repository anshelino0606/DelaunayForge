#ifndef FEM_RENDERER_COMMON_H
#define FEM_RENDERER_COMMON_H

namespace fem {

enum ViewMode {
    FEM_VIEW_MODE_3D,
    FEM_VIEW_MODE_UI
};

enum class GraphicsAPI {
    D3D12,
    METAL,
};

}

#endif // FEM_RENDERER_COMMON_H