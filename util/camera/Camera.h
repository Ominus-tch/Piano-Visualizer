#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>

#include <wincodec.h>

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

class Camera
{
public:

    Camera();
    ~Camera();

    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        int cameraIndex = 0
    );

    void Shutdown();

    // Called from the main/render thread.
    //
    // Takes the newest decoded camera frame and uploads
    // it to the D3D11 texture.
    bool Update();

    bool IsOpen() const;

    ID3D11ShaderResourceView* GetTexture() const;

    int GetWidth() const;
    int GetHeight() const;

private:

    // =========================================================
    // CAMERA THREAD
    // =========================================================

    // Runs on the camera thread.
    //
    // Performs:
    //   - IMFSourceReader::ReadSample()
    //   - MJPEG decoding
    //   - WIC conversion to BGRA
    //
    // It does NOT touch the D3D11 immediate context.
    void CameraThread();

    // Reads and decodes one camera frame.
    //
    // The decoded pixels are written into the thread-local
    // capture buffer and later published to the render thread.
    bool CaptureFrame();


    // =========================================================
    // CAMERA / MEDIA FOUNDATION
    // =========================================================

    bool FindCamera(
        int cameraIndex,
        IMFActivate** camera
    );

    bool CreateReader(
        IMFActivate* camera
    );


    // =========================================================
    // DIRECTX 11
    // =========================================================

    // Must be called from the render thread.
    bool CreateTexture();


    // Uploads a completed decoded frame to the D3D11 texture.
    //
    // Must only be called from the render thread.
    bool UploadFrame(
        const BYTE* pixels,
        size_t pixelSize
    );


    // =========================================================
    // D3D11
    // =========================================================

    ID3D11Device* m_device = nullptr;

    ID3D11DeviceContext* m_context = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        m_texture;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
        m_shaderResourceView;


    // =========================================================
    // MEDIA FOUNDATION
    // =========================================================

    Microsoft::WRL::ComPtr<IMFSourceReader>
        m_reader;

    bool m_mediaFoundationStarted = false;


    // =========================================================
    // WIC
    //
    // Used by the camera thread to decode JPEG/MJPEG data
    // into 32-bit BGRA pixels.
    // =========================================================

    Microsoft::WRL::ComPtr<IWICImagingFactory>
        m_wicFactory;


    // =========================================================
    // CAMERA THREAD BUFFERS
    //
    // captureBuffer:
    //     Owned exclusively by the camera thread while
    //     decoding a frame.
    //
    // readyBuffer:
    //     Contains the newest completed frame waiting for
    //     the render thread.
    // =========================================================

    std::vector<BYTE> m_captureBuffer;

    std::vector<BYTE> m_readyBuffer;

    std::mutex m_frameMutex;

    std::atomic<bool> m_newFrameAvailable{ false };


    // =========================================================
    // CAMERA THREAD
    // =========================================================

    std::thread m_cameraThread;

    std::atomic<bool> m_running{ false };


    // =========================================================
    // CAMERA FORMAT
    // =========================================================

    int m_width = 0;

    int m_height = 0;

    UINT32 m_fpsNumerator = 0;

    UINT32 m_fpsDenominator = 1;


    // =========================================================
    // STATE
    // =========================================================

    std::atomic<bool> m_open{ false };
};