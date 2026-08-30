#include "Framebuffer.h"

#include <cstring>

Framebuffer::Framebuffer(
    ID3D11Device* device,
    int width,
    int height
)
    : _width(width),
    _height(height)
{
    createResources(device);
}

Framebuffer::~Framebuffer()
{
    clean();
}


void Framebuffer::createResources(ID3D11Device* device)
{
    clean();

    // ------------------------------------------------------------
    // Texture
    // ------------------------------------------------------------

    D3D11_TEXTURE2D_DESC textureDesc = {};

    textureDesc.Width = _width;
    textureDesc.Height = _height;

    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;

    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;

    textureDesc.Usage = D3D11_USAGE_DEFAULT;

    textureDesc.BindFlags =
        D3D11_BIND_RENDER_TARGET |
        D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(
        &textureDesc,
        nullptr,
        &_texture
    );

    if (FAILED(hr))
    {
        return;
    }


    // ------------------------------------------------------------
    // Render Target View
    // ------------------------------------------------------------

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};

    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    rtvDesc.ViewDimension =
        D3D11_RTV_DIMENSION_TEXTURE2D;

    rtvDesc.Texture2D.MipSlice = 0;

    hr = device->CreateRenderTargetView(
        _texture,
        &rtvDesc,
        &_renderTarget
    );

    if (FAILED(hr))
    {
        clean();
        return;
    }


    // ------------------------------------------------------------
    // Shader Resource View
    // ------------------------------------------------------------

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    srvDesc.ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2D;

    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(
        _texture,
        &srvDesc,
        &_shaderResource
    );

    if (FAILED(hr))
    {
        clean();
        return;
    }
}


void Framebuffer::bind(ID3D11DeviceContext* context)
{
    context->OMSetRenderTargets(
        1,
        &_renderTarget,
        nullptr
    );
}


void Framebuffer::unbind(ID3D11DeviceContext* context)
{
    // Don't assume what the application's backbuffer is.
    // The owner of the D3D11 context should restore its own
    // render target.

    ID3D11RenderTargetView* nullRTV = nullptr;

    context->OMSetRenderTargets(
        1,
        &nullRTV,
        nullptr
    );
}


void Framebuffer::resize(
    ID3D11Device* device,
    int width,
    int height
)
{
    if (width == _width && height == _height)
        return;

    _width = width;
    _height = height;

    createResources(device);
}


void Framebuffer::resize(
    ID3D11Device* device,
    const glm::vec2& size
)
{
    resize(
        device,
        static_cast<int>(size.x),
        static_cast<int>(size.y)
    );
}

bool Framebuffer::readPixels(unsigned char* destination, size_t destinationSize)
{
    if (!destination || !_texture)
        return false;

    const size_t requiredSize =
        static_cast<size_t>(_width) *
        static_cast<size_t>(_height) *
        4;

    if (destinationSize < requiredSize)
        return false;

    ID3D11Device* device = nullptr;
    _texture->GetDevice(&device);

    if (!device)
        return false;

    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);

    if (!context)
    {
        device->Release();
        return false;
    }

    // Describe a CPU-readable staging texture.
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    _texture->GetDesc(&stagingDesc);

    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    ID3D11Texture2D* stagingTexture = nullptr;

    HRESULT hr = device->CreateTexture2D(
        &stagingDesc,
        nullptr,
        &stagingTexture
    );

    if (FAILED(hr))
    {
        context->Release();
        device->Release();
        return false;
    }

    // Copy the GPU render target into the CPU-readable texture.
    context->CopyResource(stagingTexture, _texture);

    D3D11_MAPPED_SUBRESOURCE mapped = {};

    hr = context->Map(
        stagingTexture,
        0,
        D3D11_MAP_READ,
        0,
        &mapped
    );

    if (FAILED(hr))
    {
        stagingTexture->Release();
        context->Release();
        device->Release();
        return false;
    }

    const size_t rowSize =
        static_cast<size_t>(_width) * 4;

    for (int y = 0; y < _height; ++y)
    {
        const unsigned char* source =
            static_cast<const unsigned char*>(mapped.pData)
            + static_cast<size_t>(y) * mapped.RowPitch;

        unsigned char* destinationRow =
            destination
            + static_cast<size_t>(y) * rowSize;

        memcpy(destinationRow, source, rowSize);
    }

    context->Unmap(stagingTexture, 0);

    stagingTexture->Release();
    context->Release();
    device->Release();

    return true;
}


void Framebuffer::clean()
{
    if (_shaderResource)
    {
        _shaderResource->Release();
        _shaderResource = nullptr;
    }

    if (_renderTarget)
    {
        _renderTarget->Release();
        _renderTarget = nullptr;
    }

    if (_texture)
    {
        _texture->Release();
        _texture = nullptr;
    }
}