#ifndef FEM_RENDERER_SURFACE_H
#define FEM_RENDERER_SURFACE_H

#include <LLGL/LLGL.h>

namespace fem {

class Window;

class Surface : public LLGL::Surface {
public:
    Surface(Window* window);

    virtual bool GetNativeHandle(void* nativeHandle, std::size_t nativeHandleSize) override;
    virtual LLGL::Extent2D GetContentSize() const override;
    virtual bool AdaptForVideoMode(LLGL::Extent2D* resolution, bool* fullscreen) override;
    virtual LLGL::Display* FindResidentDisplay() const override;


private:
    Window* window_ = nullptr;
};

}

#endif // FEM_RENDERER_SURFACE_H