#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <atomic>
#include <thread>
#include <mutex>
#include <vector>

class WindowCapture
{
public:
    WindowCapture();
    ~WindowCapture();

    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context
    );

    bool Start(HWND hwnd);
    void Stop();

    // Called from the main/render thread.
    // Uploads the newest captured frame to the D3D11 texture.
    bool Update();

    ID3D11ShaderResourceView* GetTexture() const;

    int GetWidth() const;
    int GetHeight() const;

private:

    // Runs on the capture thread.
    void CaptureThread();

    // Captures one frame into the CPU buffer.
    bool CaptureFrame();

    // Must be called from the render thread.
    bool CreateTexture(
        int newWidth,
        int newHeight
    );

private:

    // =========================================================
    // D3D11
    // =========================================================

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
        shaderResourceView;


    // =========================================================
    // Capture thread
    // =========================================================

    std::thread captureThread;

    std::atomic<bool> running = false;

    HWND hwnd = nullptr;


    // =========================================================
    // Persistent GDI resources
    // =========================================================

    HDC windowDC = nullptr;
    HDC memoryDC = nullptr;

    HBITMAP bitmap = nullptr;
    HGDIOBJ oldBitmap = nullptr;


    // =========================================================
    // CPU frame buffers
    //
    // captureBuffer:
    //     Written by capture thread.
    //
    // readyBuffer:
    //     Contains the latest completed frame.
    //
    // They are swapped when a frame is complete.
    // =========================================================

    std::vector<unsigned char> captureBuffer;
    std::vector<unsigned char> readyBuffer;

    std::mutex frameMutex;

    bool newFrameAvailable = false;


    // =========================================================
    // Capture dimensions
    // =========================================================

    int captureWidth = 0;
    int captureHeight = 0;


    // =========================================================
    // Render dimensions
    // =========================================================

    int width = 0;
    int height = 0;


    // =========================================================
    // Bitmap information
    // =========================================================

    BITMAPINFO bitmapInfo{};


    // =========================================================
    // GDI helpers
    // =========================================================

    bool CreateGDIResources(
        int newWidth,
        int newHeight
    );

    void DestroyGDIResources();
};