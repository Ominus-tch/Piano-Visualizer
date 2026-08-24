#include "WindowCapture.h"

#include <cstring>
#include <algorithm>
#include <chrono>


WindowCapture::WindowCapture()
{
}


WindowCapture::~WindowCapture()
{
    Stop();

    DestroyGDIResources();
}


// =============================================================
// INITIALIZE
// =============================================================

bool WindowCapture::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context
)
{
    if (!device || !context)
        return false;

    this->device = device;
    this->context = context;

    return true;
}


// =============================================================
// START
// =============================================================

bool WindowCapture::Start(HWND hwnd)
{
    if (!hwnd)
        return false;

    if (!IsWindow(hwnd))
        return false;

    if (running)
        return false;

    this->hwnd = hwnd;

    running = true;

    captureThread =
        std::thread(
            &WindowCapture::CaptureThread,
            this
        );

    return true;
}


// =============================================================
// STOP
// =============================================================

void WindowCapture::Stop()
{
    running = false;

    if (captureThread.joinable())
        captureThread.join();

    hwnd = nullptr;
}

// =============================================================
// CREATE D3D11 TEXTURE
//
// IMPORTANT:
// This is only called from the main/render thread.
// =============================================================

bool WindowCapture::CreateTexture(
    int newWidth,
    int newHeight
)
{
    if (newWidth <= 0 ||
        newHeight <= 0)
    {
        return false;
    }

    if (texture &&
        width == newWidth &&
        height == newHeight)
    {
        return true;
    }

    texture.Reset();
    shaderResourceView.Reset();

    width = newWidth;
    height = newHeight;

    D3D11_TEXTURE2D_DESC desc{};

    desc.Width =
        static_cast<UINT>(width);

    desc.Height =
        static_cast<UINT>(height);

    desc.MipLevels = 1;
    desc.ArraySize = 1;

    desc.Format =
        DXGI_FORMAT_B8G8R8A8_UNORM;

    desc.SampleDesc.Count = 1;

    desc.Usage =
        D3D11_USAGE_DYNAMIC;

    desc.BindFlags =
        D3D11_BIND_SHADER_RESOURCE;

    desc.CPUAccessFlags =
        D3D11_CPU_ACCESS_WRITE;

    HRESULT hr =
        device->CreateTexture2D(
            &desc,
            nullptr,
            &texture
        );

    if (FAILED(hr))
        return false;


    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};

    srvDesc.Format =
        DXGI_FORMAT_B8G8R8A8_UNORM;

    srvDesc.ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2D;

    srvDesc.Texture2D.MipLevels = 1;

    hr =
        device->CreateShaderResourceView(
            texture.Get(),
            &srvDesc,
            &shaderResourceView
        );

    if (FAILED(hr))
    {
        texture.Reset();

        return false;
    }

    return true;
}


// =============================================================
// CREATE GDI RESOURCES
//
// Called by the capture thread when the window size changes.
// =============================================================

bool WindowCapture::CreateGDIResources(
    int newWidth,
    int newHeight
)
{
    if (!hwnd)
        return false;

    if (newWidth <= 0 ||
        newHeight <= 0)
    {
        return false;
    }

    // Already correct size.
    if (windowDC &&
        memoryDC &&
        bitmap &&
        captureWidth == newWidth &&
        captureHeight == newHeight)
    {
        return true;
    }

    DestroyGDIResources();

    windowDC =
        GetDC(hwnd);

    if (!windowDC)
        return false;

    memoryDC =
        CreateCompatibleDC(windowDC);

    if (!memoryDC)
    {
        ReleaseDC(
            hwnd,
            windowDC
        );

        windowDC = nullptr;

        return false;
    }

    bitmap =
        CreateCompatibleBitmap(
            windowDC,
            newWidth,
            newHeight
        );

    if (!bitmap)
    {
        DeleteDC(memoryDC);
        ReleaseDC(hwnd, windowDC);

        memoryDC = nullptr;
        windowDC = nullptr;

        return false;
    }

    oldBitmap =
        SelectObject(
            memoryDC,
            bitmap
        );

    if (!oldBitmap)
    {
        DeleteObject(bitmap);
        DeleteDC(memoryDC);
        ReleaseDC(hwnd, windowDC);

        bitmap = nullptr;
        memoryDC = nullptr;
        windowDC = nullptr;

        return false;
    }

    captureWidth = newWidth;
    captureHeight = newHeight;


    // ---------------------------------------------------------
    // Persistent pixel buffer
    // ---------------------------------------------------------

    const size_t bufferSize =
        static_cast<size_t>(newWidth) *
        static_cast<size_t>(newHeight) *
        4;

    captureBuffer.resize(bufferSize);
    readyBuffer.resize(bufferSize);


    // ---------------------------------------------------------
    // BITMAPINFO
    // ---------------------------------------------------------

    std::memset(
        &bitmapInfo,
        0,
        sizeof(bitmapInfo)
    );

    bitmapInfo.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);

    bitmapInfo.bmiHeader.biWidth =
        newWidth;

    // Top-down bitmap.
    bitmapInfo.bmiHeader.biHeight =
        -newHeight;

    bitmapInfo.bmiHeader.biPlanes =
        1;

    bitmapInfo.bmiHeader.biBitCount =
        32;

    bitmapInfo.bmiHeader.biCompression =
        BI_RGB;

    return true;
}


// =============================================================
// DESTROY GDI RESOURCES
// =============================================================

void WindowCapture::DestroyGDIResources()
{
    if (memoryDC &&
        oldBitmap)
    {
        SelectObject(
            memoryDC,
            oldBitmap
        );

        oldBitmap = nullptr;
    }

    if (bitmap)
    {
        DeleteObject(bitmap);

        bitmap = nullptr;
    }

    if (memoryDC)
    {
        DeleteDC(memoryDC);

        memoryDC = nullptr;
    }

    if (windowDC)
    {
        ReleaseDC(
            hwnd,
            windowDC
        );

        windowDC = nullptr;
    }

    captureWidth = 0;
    captureHeight = 0;
}

// =============================================================
// CAPTURE THREAD
// =============================================================

void WindowCapture::CaptureThread()
{
    constexpr double TARGET_FPS = 30.0;

    const auto frameDuration =
        std::chrono::duration_cast<
        std::chrono::steady_clock::duration
        >(
            std::chrono::duration<double>(
                1.0 / TARGET_FPS
            )
        );

    auto nextFrameTime =
        std::chrono::steady_clock::now();

    while (running)
    {
        // -----------------------------------------------------
        // Wait for next frame
        // -----------------------------------------------------

        std::this_thread::sleep_until(
            nextFrameTime
        );

        if (!running)
            break;

        // Schedule the next frame.
        nextFrameTime += frameDuration;


        // -----------------------------------------------------
        // If capture took too long, don't try to catch up.
        // Start a new frame interval from now.
        // -----------------------------------------------------

        if (nextFrameTime <
            std::chrono::steady_clock::now())
        {
            nextFrameTime =
                std::chrono::steady_clock::now() +
                frameDuration;
        }


        // -----------------------------------------------------
        // Check window
        // -----------------------------------------------------

        if (!IsWindow(hwnd))
            continue;


        // -----------------------------------------------------
        // Get current window size
        // -----------------------------------------------------

        RECT rect{};

        if (!GetClientRect(
            hwnd,
            &rect))
        {
            continue;
        }

        const int newWidth =
            rect.right - rect.left;

        const int newHeight =
            rect.bottom - rect.top;

        if (newWidth <= 0 ||
            newHeight <= 0)
        {
            continue;
        }


        // -----------------------------------------------------
        // Create/recreate GDI resources if needed
        // -----------------------------------------------------

        if (!CreateGDIResources(
            newWidth,
            newHeight))
        {
            continue;
        }


        // -----------------------------------------------------
        // Print window
        // -----------------------------------------------------

        BOOL result =
            PrintWindow(
                hwnd,
                memoryDC,
                PW_CLIENTONLY
            );


        // -----------------------------------------------------
        // Fallback
        // -----------------------------------------------------

        if (!result)
        {
            result =
                BitBlt(
                    memoryDC,
                    0,
                    0,
                    captureWidth,
                    captureHeight,
                    windowDC,
                    0,
                    0,
                    SRCCOPY
                );
        }

        if (!result)
            continue;


        // -----------------------------------------------------
        // Convert bitmap → CPU pixels
        // -----------------------------------------------------

        int scanLines =
            GetDIBits(
                memoryDC,
                bitmap,
                0,
                captureHeight,
                captureBuffer.data(),
                &bitmapInfo,
                DIB_RGB_COLORS
            );

        if (scanLines != captureHeight)
            continue;


        // -----------------------------------------------------
        // Publish completed frame
        // -----------------------------------------------------

        {
            std::lock_guard<std::mutex> lock(
                frameMutex
            );

            std::swap(
                captureBuffer,
                readyBuffer
            );

            newFrameAvailable = true;
        }
    }
}


// =============================================================
// UPDATE
//
// MAIN / RENDER THREAD ONLY
//
// This is where we interact with D3D11.
// =============================================================

bool WindowCapture::Update()
{
    // ---------------------------------------------------------
    // Check for a new frame
    // ---------------------------------------------------------

    {
        std::lock_guard<std::mutex> lock(
            frameMutex
        );

        if (!newFrameAvailable)
            return false;

        newFrameAvailable = false;
    }


    // ---------------------------------------------------------
    // Make sure texture exists
    // ---------------------------------------------------------

    if (!CreateTexture(
        captureWidth,
        captureHeight))
    {
        return false;
    }


    // ---------------------------------------------------------
    // Map D3D11 texture
    // ---------------------------------------------------------

    D3D11_MAPPED_SUBRESOURCE mapped{};

    HRESULT hr =
        context->Map(
            texture.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped
        );

    if (FAILED(hr))
        return false;


    // ---------------------------------------------------------
    // Copy rows
    // ---------------------------------------------------------

    const size_t rowBytes =
        static_cast<size_t>(
            captureWidth
            ) * 4;


    const unsigned char* source =
        readyBuffer.data();


    unsigned char* destination =
        static_cast<unsigned char*>(
            mapped.pData
            );


    for (int y = 0;
        y < captureHeight;
        ++y)
    {
        std::memcpy(
            destination +
            static_cast<size_t>(y) *
            mapped.RowPitch,

            source +
            static_cast<size_t>(y) *
            rowBytes,

            rowBytes
        );
    }


    // ---------------------------------------------------------
    // Done
    // ---------------------------------------------------------

    context->Unmap(
        texture.Get(),
        0
    );

    return true;
}


// =============================================================
// GET TEXTURE
// =============================================================

ID3D11ShaderResourceView*
WindowCapture::GetTexture() const
{
    return shaderResourceView.Get();
}


// =============================================================
// GET WIDTH
// =============================================================

int WindowCapture::GetWidth() const
{
    return width;
}


// =============================================================
// GET HEIGHT
// =============================================================

int WindowCapture::GetHeight() const
{
    return height;
}