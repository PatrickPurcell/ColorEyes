
/*
==========================================
  Copyright (c) 2022 Dynamic_Static
      Licensed under the MIT license
    http://opensource.org/licenses/MIT
==========================================
*/

#include "dynamic_static/d3d11.hpp"
#include "dynamic_static/math.hpp"
#include "dynamic_static/opengl.hpp"
#include "dynamic_static/system.hpp"

#include <array>
#include <cassert>
#include <iostream>

class StagingResource final
{
public:
    static HRESULT create(const dst::d3d11::ComPtr<ID3D11Device>& cpD3d11Device,  const dst::d3d11::ComPtr<ID3D11DeviceContext>& cpD3d11DeviceContext, StagingResource* pStagingResource)
    {
        assert(pStagingResource);
        pStagingResource->reset();
        pStagingResource->mcpD3d11Device = cpD3d11Device;
        pStagingResource->mcpD3d11DeviceContext = cpD3d11DeviceContext;
        return pStagingResource->mcpD3d11Device && pStagingResource->mcpD3d11DeviceContext ? S_OK : S_FALSE;
    }

    StagingResource() = default;
    StagingResource(StagingResource&&) = default;
    StagingResource& operator=(StagingResource&&) = default;

    ~StagingResource()
    {
        reset();
    }

    void reset()
    {
        mImage.clear();
        mcpD3d11Device = nullptr;
        mcpD3d11DeviceContext = nullptr;
        mcpDstD3d11Texture2d = nullptr;
    }

    const dst::sys::Image& get_image() const
    {
        return mImage;
    }

    HRESULT update(const glm::u32vec2& offset, const glm::u32vec2& extent, const dst::d3d11::ComPtr<ID3D11Texture2D>& cpSrcD3d11Texture2d)
    {
        dst_h_result_scope_begin {
            auto currentExtent = mImage.get_create_info().extent;
            if (!mcpDstD3d11Texture2d || currentExtent.x != extent.x || currentExtent.y != extent.y) {
                D3D11_TEXTURE2D_DESC d3d11Texture2dDesc { };
                cpSrcD3d11Texture2d->GetDesc(&d3d11Texture2dDesc);
                d3d11Texture2dDesc.Width = extent.x;
                d3d11Texture2dDesc.Height = extent.y;
                d3d11Texture2dDesc.Usage = D3D11_USAGE_STAGING;
                d3d11Texture2dDesc.BindFlags = 0;
                d3d11Texture2dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
                d3d11Texture2dDesc.MiscFlags = 0;
                dst_h_result(mcpD3d11Device->CreateTexture2D(&d3d11Texture2dDesc, nullptr, &mcpDstD3d11Texture2d));
                dst::sys::Image::CreateInfo dstSysImageCreateInfo { };
                dstSysImageCreateInfo.extent = { extent.x, extent.y, 1 };
                dstSysImageCreateInfo.format = dst::sys::Format::R8G8B8A8_UNorm;
                dst::sys::Image::create(&dstSysImageCreateInfo, &mImage);
            }
            D3D11_BOX srcBox {
                srcBox.left     = offset.x,
                srcBox.top      = offset.y,
                srcBox.front    = 0,
                srcBox.right    = offset.x + extent.x,
                srcBox.bottom   = offset.y + extent.y,
                srcBox.back     = 1,
            };
            mcpD3d11DeviceContext->CopySubresourceRegion(mcpDstD3d11Texture2d.Get(), 0, 0, 0, 0, cpSrcD3d11Texture2d.Get(), 0, &srcBox);
            D3D11_MAPPED_SUBRESOURCE d3d11MappedSubresource { };
            dst_h_result(mcpD3d11DeviceContext->Map(mcpDstD3d11Texture2d.Get(), 0, D3D11_MAP_READ, 0, &d3d11MappedSubresource));
            for (uint32_t row = 0; row < extent.y; ++row) {
                memcpy(&mImage[0] + row * extent.x * 4, (const uint8_t*)d3d11MappedSubresource.pData + d3d11MappedSubresource.RowPitch * row, extent.x * 4);
            }
            mcpD3d11DeviceContext->Unmap(mcpDstD3d11Texture2d.Get(), 0);
        } dst_h_result_scope_end;
        return hResult;
    }

private:
    dst::sys::Image mImage;
    dst::d3d11::ComPtr<ID3D11Device> mcpD3d11Device;
    dst::d3d11::ComPtr<ID3D11DeviceContext> mcpD3d11DeviceContext;
    dst::d3d11::ComPtr<ID3D11Texture2D> mcpDstD3d11Texture2d;
    StagingResource(const StagingResource&) = delete;
    StagingResource& operator=(const StagingResource&) = delete;
};

class Win32DesktopDuplicationManager final
{
public:
    static HRESULT create(Win32DesktopDuplicationManager* pWin32DesktopDuplicationManager)
    {
        assert(pWin32DesktopDuplicationManager);
        pWin32DesktopDuplicationManager->reset();
        dst_h_result_scope_begin {
            dst_h_result(pWin32DesktopDuplicationManager->create_device());
            dst_h_result(StagingResource::create(pWin32DesktopDuplicationManager->mcpD3d11Device, pWin32DesktopDuplicationManager->mcpD3d11DeviceContext, &pWin32DesktopDuplicationManager->mStagingResource));
            dst_h_result(pWin32DesktopDuplicationManager->create_output_duplication());
        } dst_h_result_scope_end;
        return hResult;
    }

    Win32DesktopDuplicationManager() = default;
    Win32DesktopDuplicationManager(Win32DesktopDuplicationManager&&) = default;
    Win32DesktopDuplicationManager& operator=(Win32DesktopDuplicationManager&&) = default;

    ~Win32DesktopDuplicationManager()
    {
        reset();
    }

    void reset()
    {
        mStagingResource.reset();
        mcpD3d11Device = nullptr;
        mcpD3d11DeviceContext = nullptr;
        mcpDxgiOutputDuplication = nullptr;
    }

    const StagingResource& get_staging_resource() const
    {
        return mStagingResource;
    }

    HRESULT update(const glm::ivec2& offset, const glm::ivec2& extent)
    {
        dst_h_result_scope_begin {
            DXGI_OUTDUPL_FRAME_INFO dxgiOutduplFrameInfo { };
            dst::d3d11::ComPtr<IDXGIResource> cpDxgiResource;
            dst_h_result(mcpDxgiOutputDuplication->AcquireNextFrame(0, &dxgiOutduplFrameInfo, &cpDxgiResource));
            dst::d3d11::ComPtr<ID3D11Texture2D> cpSrcD3d11Texture2d;
            dst_h_result(cpDxgiResource.As(&cpSrcD3d11Texture2d));
            dst_h_result(mStagingResource.update(offset, extent, cpSrcD3d11Texture2d));
            dst_h_result(mcpDxgiOutputDuplication->ReleaseFrame());
        } dst_h_result_scope_end;
        return hResult;
    }

private:
    HRESULT create_device()
    {
        std::array<D3D_DRIVER_TYPE, 4> d3dDriverTypes {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_REFERENCE,
            D3D_DRIVER_TYPE_SOFTWARE,
            D3D_DRIVER_TYPE_WARP,
        };
        std::array<D3D_FEATURE_LEVEL, 4> d3dFeatureLevels {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_1,
        };
        HRESULT hResult = S_FALSE;
        for (auto d3dDriverType : d3dDriverTypes) {
            hResult = D3D11CreateDevice(nullptr, d3dDriverType, NULL, D3D11_CREATE_DEVICE_DEBUG, d3dFeatureLevels.data(), (UINT)d3dFeatureLevels.size(), D3D11_SDK_VERSION, &mcpD3d11Device, nullptr, &mcpD3d11DeviceContext);
            if (hResult == S_OK) {
                break;
            }
        }
        return hResult;
    }

    HRESULT create_output_duplication()
    {
        dst_h_result_scope_begin {
            dst::d3d11::ComPtr<IDXGIDevice> cpDxgiDevice;
            dst_h_result(mcpD3d11Device.As(&cpDxgiDevice));
            dst::d3d11::ComPtr<IDXGIAdapter> cpDxgiAdapter;
            dst_h_result(cpDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)cpDxgiAdapter.GetAddressOf()));
            dst::d3d11::ComPtr<IDXGIOutput> cpDxgiOutput;
            dst_h_result(cpDxgiAdapter->EnumOutputs(0, cpDxgiOutput.GetAddressOf()));
            dst::d3d11::ComPtr<IDXGIOutput1> cpDxgiOutput1;
            dst_h_result(cpDxgiOutput.As(&cpDxgiOutput1));
            dst_h_result(cpDxgiOutput1->DuplicateOutput(mcpD3d11Device.Get(), mcpDxgiOutputDuplication.GetAddressOf()));
        } dst_h_result_scope_end;
        return hResult;
    }

    StagingResource mStagingResource;
    dst::d3d11::ComPtr<ID3D11Device> mcpD3d11Device;
    dst::d3d11::ComPtr<ID3D11DeviceContext> mcpD3d11DeviceContext;
    dst::d3d11::ComPtr<IDXGIOutputDuplication> mcpDxgiOutputDuplication;
    Win32DesktopDuplicationManager(const Win32DesktopDuplicationManager&) = delete;
    Win32DesktopDuplicationManager& operator=(const Win32DesktopDuplicationManager&) = delete;
};

struct Vertex final
{
    glm::vec2 position { };
    glm::vec2 texcoord { };
};

namespace dst {
namespace gl {

template <>
void enable_vertex_attributes<Vertex>()
{
    enable_vertex_attributes<Vertex, 2>({{
        { GL_FLOAT, 2 },
        { GL_FLOAT, 2 }
    }});
}

} // namespace gl
} // namespace dst

int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;
    using namespace dst::sys;
    Window::Info windowInfo { };
    windowInfo.flags |= Window::Info::Flags::Transparent;
    dst::gl::Context::Info glContextInfo { };
    windowInfo.pGlContextInfo = &glContextInfo;
    Window window(windowInfo);

    bool closeRequested = false;
    dst::Delegate<const Window&> onCloseRequested;
    window.on_close_requested += onCloseRequested;
    onCloseRequested = [&](const auto&) { closeRequested = true; };

    dst::Clock clock;

    Win32DesktopDuplicationManager win32DesktopDuplicationManager;
    auto hResult = Win32DesktopDuplicationManager::create(&win32DesktopDuplicationManager);
    assert(SUCCEEDED(hResult));
    dst::gl::Texture desktopTexture;
    dst::gl::Framebuffer desktopFramebuffer;

    dst::gl::Gui gui;

    // TODO : Documentation
    //  -1,1      0,1      1,1
    //    +--------+--------+
    //    |(NDC)   |(TC)    |
    //    |        |        |
    //    |        |        |
    //    |        +--------+
    //    |       0,0      1,0
    //    |                 |
    //    |                 |
    //    +-----------------+
    //  -1,-1              1,-1
    std::array<Vertex, 4> vertices {
        Vertex { .position = { -1.0f,  1.0f }, .texcoord = { 0.0f, 1.0f } },
        Vertex { .position = {  1.0f,  1.0f }, .texcoord = { 1.0f, 1.0f } },
        Vertex { .position = {  1.0f, -1.0f }, .texcoord = { 1.0f, 0.0f } },
        Vertex { .position = { -1.0f, -1.0f }, .texcoord = { 0.0f, 0.0f } },
    };
    std::array<GLushort, 6> indices {
        0, 1, 2,
        0, 2, 3,
    };
    dst::gl::Mesh mesh;
    mesh.write<Vertex, GLushort>((GLsizei)vertices.size(), vertices.data(), (GLsizei)indices.size(), indices.data());

    // NOTE : We're creating a Program here...TODO
    std::array<dst::gl::Shader, 2> shaders {{
        {
            // ...This is a vertex shader.  We're going to use it to process each of the|||
            //  vertices of the quad mesh we set up above.  TODO...
            GL_VERTEX_SHADER,
            __LINE__,
            R"(
                #version 330

                in vec2 vsPosition;
                in vec2 vsTexcoord;
                out vec2 fsTexcoord;

                void main()
                {
                    gl_Position = vec4(vsPosition.x, vsPosition.y, 0, 1);
                    fsTexcoord = vsTexcoord;
                }
            )"
        },
        {
// ...This is a fragment shader.  It's used
            GL_FRAGMENT_SHADER,
            __LINE__,
            R"(
                #version 330

                uniform sampler2D desktopTexture;
                in vec2 fsTexcoord;
                out vec4 fragColor;

                void main()
                {
                    fragColor = texture(desktopTexture, fsTexcoord);
                }
            )"
        }
    }};
    dst::gl::Program program((GLsizei)shaders.size(), shaders.data());

    

    glm::ivec2 guiExtent { 1, 1 };

    // TODO : Documentation
    while (!closeRequested && window.get_input().keyboard.up(Key::Escape)) {
        // TODO : Documentation
        Window::poll_events();
        clock.update();

        // TODO : Documentation
        win32DesktopDuplicationManager.update({ 0, 0 }, guiExtent);

        if (1 < guiExtent.x && 1 < guiExtent.y) {
            // TODO : Documentation
            // NOTE : This is kinda crappy...I'm pulling the desktop pixel data off the GPU
            //  with D3D11/DXGI, then just immediately reuploading it via OpenGL.  All of
            //  this code should be ported to D3D11 but I wanted to get this going quick
            //  and I had a bunch of OpenGL code ready...
            if (desktopTexture.get_info().width != guiExtent.x || desktopTexture.get_info().height != guiExtent.y) {
                dst::gl::Texture::Info textureInfo { };
                textureInfo.format = GL_RGBA;
                textureInfo.width = guiExtent.x;
                textureInfo.height = guiExtent.y;
                desktopTexture = dst::gl::Texture(textureInfo);
                dst::gl::Framebuffer::CreateInfo framebufferCreateInfo { };
                framebufferCreateInfo.colorAttachmentCreateInfoCount = 1;
                framebufferCreateInfo.pColorAttachmentCreateInfos = &textureInfo;
                // dst::gl::Framebuffer::create(&framebufferCreateInfo, &desktopFramebuffer);
            }
            desktopTexture.write(&win32DesktopDuplicationManager.get_staging_resource().get_image()[0]);

            // GLint currentFramebufferBinding = 0;
            // dst_gl(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFramebufferBinding));
            // dst_gl(glBindFramebuffer(GL_FRAMEBUFFER, desktopFramebuffer.get_handle()));
            // dst_gl(glViewport(0, 0, (GLsizei)guiExtent[0], (GLsizei)guiExtent[1]));
            // dst_gl(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
            // dst_gl(glClear(GL_COLOR_BUFFER_BIT));
            // program.bind();
            // dst_gl(glActiveTexture(GL_TEXTURE0));
            // desktopTexture.bind();
            // mesh.draw_indexed();
            // program.unbind();
            // dst_gl(glBindFramebuffer(GL_FRAMEBUFFER, currentFramebufferBinding));
        }

        // TODO : Documentation
        const auto& extent = window.get_info().extent;
        dst_gl(glViewport(0, 0, (GLsizei)extent[0], (GLsizei)extent[1]));
        dst_gl(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
        dst_gl(glClear(GL_COLOR_BUFFER_BIT));

        // TODO : Documentation
        gui.begin_frame(window, clock.elapsed<dst::Seconds<float>>());
        // ImGui::ShowDemoWindow();

        // TODO : Documentation
        if (1 < guiExtent.x && 1 < guiExtent.y) {
            #if 0
            ImGui::Image((ImTextureID)&desktopFramebuffer.get_color_attachments()[0], { (float)guiExtent.x, (float)guiExtent.y });
            #else
            ImGui::Image((ImTextureID)&desktopTexture, { (float)guiExtent.x, (float)guiExtent.y });
            #endif
        }

        // TODO : Documentation
        auto windowSize = ImGui::GetWindowSize();
        guiExtent.x = (GLsizei)std::max(1, (int)windowSize.x - 8);
        guiExtent.y = (GLsizei)std::max(1, (int)windowSize.y - 8);

        // TODO : Documentation
        gui.end_frame();
        gui.draw();
        window.swap();
    }
    return 0;
}
