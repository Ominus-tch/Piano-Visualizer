#pragma once

#include <d3d11.h>
#include <glm/glm.hpp>

class Framebuffer
{
public:

    Framebuffer(
        ID3D11Device* device,
        int width,
        int height
    );

    ~Framebuffer();

    // Bind this framebuffer as the current render target.
    void bind(ID3D11DeviceContext* context);

    // Restore another render target / backbuffer.
    void unbind(ID3D11DeviceContext* context);

    // Resize the framebuffer.
    void resize(
        ID3D11Device* device,
        int width,
        int height
    );

    void resize(
        ID3D11Device* device,
        const glm::vec2& size
    );

    // Release D3D11 resources.
    void clean();

    ID3D11ShaderResourceView* textureId() const
    {
        return _shaderResource;
    }

    // The texture itself.
    ID3D11Texture2D* texture() const
    {
        return _texture;
    }

    // Used when this framebuffer is rendered into.
    ID3D11RenderTargetView* renderTarget() const
    {
        return _renderTarget;
    }

    // Used when this framebuffer is sampled by another shader.
    ID3D11ShaderResourceView* shaderResource() const
    {
        return _shaderResource;
    }

    bool readPixels(unsigned char* destination, size_t destinationSize);

    int width() const
    {
        return _width;
    }

    int height() const
    {
        return _height;
    }


private:

    void createResources(ID3D11Device* device);

    ID3D11Texture2D* _texture = nullptr;

    ID3D11RenderTargetView* _renderTarget = nullptr;

    ID3D11ShaderResourceView* _shaderResource = nullptr;

    int _width = 0;
    int _height = 0;
};