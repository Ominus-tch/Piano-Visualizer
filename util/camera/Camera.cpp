#include "Camera.h"

#include <Windows.h>
#include <wincodec.h>

#include "../Logger.h"

#include <cstring>
#include <cstdint>
#include <string>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

#define LOG_UPLOAD(step) \
    Logger::Log("UploadFrame: " step "\n")

#define LOG_UPLOAD_HR(step, hr) \
    Logger::Log( \
        std::string("UploadFrame: " step " HRESULT=0x") + \
        std::to_string(static_cast<unsigned long>(hr)) + \
        "\n" \
    )


Camera::Camera()
{
}


Camera::~Camera()
{
    Shutdown();
}


// =========================================================
// Initialize
// =========================================================

bool Camera::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    int cameraIndex,
    int width,
    int height
)
{
    if (!device || !context)
    {
        Logger::Log(
            "Camera: Invalid D3D11 device/context.\n"
        );

        return false;
    }

    m_device = device;
    m_context = context;

    // -----------------------------------------------------
    // Initialize Media Foundation
    // -----------------------------------------------------

    HRESULT hr = MFStartup(
        MF_VERSION,
        MFSTARTUP_FULL
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: MFStartup failed. HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        return false;
    }

    m_mediaFoundationStarted = true;

    // -----------------------------------------------------
    // Initialize WIC
    // -----------------------------------------------------

    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_wicFactory)
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: Failed to create WIC imaging factory. "
            "HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        Shutdown();

        return false;
    }

    Logger::Log(
        "Camera: WIC initialized.\n"
    );

    // -----------------------------------------------------
    // Find camera
    // -----------------------------------------------------

    IMFActivate* camera = nullptr;

    if (!FindCamera(
        cameraIndex,
        &camera
    ))
    {
        Logger::Log(
            "Camera: Failed to find camera.\n"
        );

        Shutdown();

        return false;
    }

    // -----------------------------------------------------
    // Create source reader
    // -----------------------------------------------------

    if (!CreateReader(
        camera,
        width,
        height
    ))
    {
        camera->Release();

        Logger::Log(
            "Camera: Failed to create source reader.\n"
        );

        Shutdown();

        return false;
    }

    camera->Release();

    // -----------------------------------------------------
    // Create D3D11 texture
    //
    // This stays on the initialization/render side.
    // The camera thread will NOT access the D3D11 context.
    // -----------------------------------------------------

    if (!CreateTexture())
    {
        Logger::Log(
            "Camera: Failed to create D3D11 texture.\n"
        );

        Shutdown();

        return false;
    }

    // -----------------------------------------------------
    // Prepare camera thread state
    // -----------------------------------------------------

    m_captureBuffer.clear();
    m_readyBuffer.clear();

    m_newFrameAvailable = false;

    // -----------------------------------------------------
    // Start camera
    // -----------------------------------------------------

    m_open = true;
    m_running = true;

    m_cameraThread =
        std::thread(
            &Camera::CameraThread,
            this
        );

    Logger::Log(
        "Camera initialized: " +
        std::to_string(m_width) +
        "x" +
        std::to_string(m_height) +
        "\n"
    );

    Logger::Log(
        "Camera: Capture thread started.\n"
    );

    return true;
}


// =========================================================
// FindCamera
// =========================================================

bool Camera::FindCamera(
    int cameraIndex,
    IMFActivate** camera
)
{
    if (!camera)
        return false;

    *camera = nullptr;

    Microsoft::WRL::ComPtr<IMFAttributes> attributes;

    HRESULT hr = MFCreateAttributes(
        &attributes,
        1
    );

    if (FAILED(hr))
        return false;

    hr = attributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );

    if (FAILED(hr))
        return false;

    IMFActivate** devices = nullptr;

    UINT32 deviceCount = 0;

    hr = MFEnumDeviceSources(
        attributes.Get(),
        &devices,
        &deviceCount
    );

    if (FAILED(hr))
        return false;

    if (deviceCount == 0)
    {
        Logger::Log(
            "Camera: No video capture devices found.\n"
        );

        CoTaskMemFree(devices);

        return false;
    }

    Logger::Log(
        "Camera: Found " +
        std::to_string(deviceCount) +
        " camera(s).\n"
    );

    if (
        cameraIndex < 0 ||
        cameraIndex >= static_cast<int>(deviceCount)
        )
    {
        Logger::Log(
            "Camera: Invalid camera index, using camera 0.\n"
        );

        cameraIndex = 0;
    }

    *camera = devices[cameraIndex];

    Logger::Log(
        "Camera: Using camera index " +
        std::to_string(cameraIndex) +
        ".\n"
    );

    // -----------------------------------------------------
    // Release every camera except the selected one.
    // -----------------------------------------------------

    for (UINT32 i = 0; i < deviceCount; ++i)
    {
        if (i != static_cast<UINT32>(cameraIndex))
        {
            devices[i]->Release();
        }
    }

    CoTaskMemFree(devices);

    return true;
}


// =========================================================
// CreateReader
// =========================================================

bool Camera::CreateReader(
    IMFActivate* camera,
    int width,
    int height
)
{
    if (!camera)
        return false;

    Microsoft::WRL::ComPtr<IMFMediaSource> source;

    HRESULT hr = camera->ActivateObject(
        IID_PPV_ARGS(&source)
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: Failed to activate camera source. "
            "HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        return false;
    }

    // -----------------------------------------------------
    // Create Source Reader attributes
    // -----------------------------------------------------

    Microsoft::WRL::ComPtr<IMFAttributes> attributes;

    hr = MFCreateAttributes(
        &attributes,
        2
    );

    if (FAILED(hr))
        return false;

    // Do NOT enable video processing.
    //
    // We want the native compressed MJPG stream from
    // the camera and will decode it ourselves using WIC.

    hr = attributes->SetUINT32(
        MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,
        FALSE
    );

    if (FAILED(hr))
        return false;

    hr = MFCreateSourceReaderFromMediaSource(
        source.Get(),
        attributes.Get(),
        &m_reader
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: Failed to create source reader. "
            "HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        return false;
    }

    // -----------------------------------------------------
    // Enumerate native MJPG formats
    // -----------------------------------------------------


    Microsoft::WRL::ComPtr<IMFMediaType> bestType;

    UINT32 bestWidth = 0;
    UINT32 bestHeight = 0;

    UINT32 bestFpsNumerator = 0;
    UINT32 bestFpsDenominator = 1;

    UINT32 typeIndex = 0;

    // Some camera drivers have broken media-type
    // enumeration and may never return MF_E_NO_MORE_TYPES.
    //
    // This prevents an infinite loop.

    constexpr UINT32 MAX_MEDIA_TYPES = 1000;

    // -----------------------------------------------------
    // We specifically prefer:
    //
    // 1920 x 1080 @ 30 FPS MJPG
    //
    // If that exact mode exists, we stop immediately.
    // -----------------------------------------------------

    constexpr UINT32 TARGET_WIDTH = 1920;
    constexpr UINT32 TARGET_HEIGHT = 1080;
    constexpr UINT32 TARGET_FPS = 30;
	const char* TARGET_FORMAT = "MJPG";

    Logger::Log(
        "Camera: Searching for " 
        + std::string(TARGET_FORMAT) 
        + " formats ("
        + std::to_string(TARGET_WIDTH)
        + "x"
        + std::to_string(TARGET_HEIGHT)
		+ " @ "
		+ std::to_string(TARGET_FPS)
        +")\n"
    );

    while (typeIndex < MAX_MEDIA_TYPES)
    {
        Microsoft::WRL::ComPtr<IMFMediaType> type;

        hr = m_reader->GetNativeMediaType(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            typeIndex,
            &type
        );

        // Normal end of enumeration.
        if (hr == static_cast<HRESULT>(0xC00D36B9L))
        {
            Logger::Log(
                "Camera: Reached end of native media types at index " +
                std::to_string(typeIndex) +
                ".\n"
            );

            break;
        }

        // Any other failure.
        if (FAILED(hr))
        {
            Logger::Log(
                "Camera: GetNativeMediaType failed at index " +
                std::to_string(typeIndex) +
                ". HRESULT: 0x" +
                std::to_string(
                    static_cast<unsigned long>(hr)
                ) +
                "\n"
            );

            break;
        }

        // -------------------------------------------------
        // Get subtype
        // -------------------------------------------------

        GUID subtype{};

        hr = type->GetGUID(
            MF_MT_SUBTYPE,
            &subtype
        );

        if (FAILED(hr))
        {
            ++typeIndex;
            continue;
        }

        // -------------------------------------------------
        // We ONLY care about MJPG.
        // -------------------------------------------------

		std::string format = "Unknown";
        if (subtype == MFVideoFormat_MJPG)
        {
			format = "MJPG";
        }
        else if (subtype == MFVideoFormat_NV12)
        {
            format = "NV12";
        }
        else if (subtype == MFVideoFormat_YUY2)
        {
            format = "YUY2";
        }
        else if (subtype == MFVideoFormat_RGB32)
        {
            format = "RGB32";
        }
        else if (subtype == MFVideoFormat_ARGB32)
        {
            format = "ARGB32";
        }
        else
        {
            wchar_t guidString[64]{};

            StringFromGUID2(
                subtype,
                guidString,
                ARRAYSIZE(guidString)
            );
            
            format = std::string(
                guidString,
                guidString + wcslen(guidString)
			);

            Logger::Log(
                "Camera: subtype = " +
                format +
                "\n"
            );
        }

        if (format != TARGET_FORMAT)
        {
            ++typeIndex;
            continue;
        }

        // -------------------------------------------------
        // Get resolution
        // -------------------------------------------------

        UINT32 formatWidth = 0;
        UINT32 formatHeight = 0;

        hr = MFGetAttributeSize(
            type.Get(),
            MF_MT_FRAME_SIZE,
            &formatWidth,
            &formatHeight
        );

        if (FAILED(hr))
        {
            ++typeIndex;
            continue;
        }

        // -------------------------------------------------
        // Get FPS
        // -------------------------------------------------

        UINT32 fpsNumerator = 0;
        UINT32 fpsDenominator = 1;

        hr = MFGetAttributeRatio(
            type.Get(),
            MF_MT_FRAME_RATE,
            &fpsNumerator,
            &fpsDenominator
        );

        if (FAILED(hr))
        {
            ++typeIndex;
            continue;
        }

        double fps =
            static_cast<double>(fpsNumerator) /
            static_cast<double>(fpsDenominator);

        // -------------------------------------------------
        // Log format
        // -------------------------------------------------

        Logger::Log(
            "Camera: "+ format + " " +
            std::to_string(typeIndex) +
            ": " +
            std::to_string(formatWidth) +
            "x" +
            std::to_string(formatHeight) +
            " @ " +
            std::to_string(fpsNumerator) +
            "/" +
            std::to_string(fpsDenominator) +
            " FPS\n"
        );

        // -------------------------------------------------
        // Ignore extremely low FPS modes
        // -------------------------------------------------

        if (fps < 15.0)
        {
            ++typeIndex;
            continue;
        }

        // -------------------------------------------------
        // EXACT TARGET
        //
        // 1920x1080 @ 30 FPS
        //
        // We want this over 1280x720 @ 60 FPS.
        // -------------------------------------------------

        bool exactTarget =
            formatWidth == TARGET_WIDTH &&
            formatHeight == TARGET_HEIGHT &&
            fpsNumerator == TARGET_FPS &&
            fpsDenominator == 1;

        if (exactTarget)
        {
            bestType = type;

            bestWidth = formatWidth;
            bestHeight = formatHeight;

            bestFpsNumerator = fpsNumerator;
            bestFpsDenominator = fpsDenominator;

            Logger::Log(
                "Camera: FOUND TARGET FORMAT: " +
                std::to_string(bestWidth) +
                "x" +
                std::to_string(bestHeight) +
                " @ " +
                std::to_string(bestFpsNumerator) +
                "/" +
                std::to_string(bestFpsDenominator) +
                " FPS (" + format + ")\n"
            );

            // We found exactly what we wanted.
            break;
        }

        // -------------------------------------------------
        // Fallback selection
        //
        // If 1920x1080 @ 30 isn't found:
        //
        // 1. Highest resolution
        // 2. Highest FPS at that resolution
        // -------------------------------------------------

        uint64_t pixels =
            static_cast<uint64_t>(formatWidth) *
            static_cast<uint64_t>(formatHeight);

        uint64_t bestPixels =
            static_cast<uint64_t>(bestWidth) *
            static_cast<uint64_t>(bestHeight);

        double bestFps = 0.0;

        if (bestFpsNumerator != 0)
        {
            bestFps =
                static_cast<double>(bestFpsNumerator) /
                static_cast<double>(bestFpsDenominator);
        }

        bool better = false;

        // First valid format.
        if (!bestType)
        {
            better = true;
        }
        // Prefer higher resolution.
        else if (pixels > bestPixels)
        {
            better = true;
        }
        // Same resolution -> prefer higher FPS.
        else if (
            pixels == bestPixels &&
            fps > bestFps
            )
        {
            better = true;
        }

        if (better)
        {
            bestType = type;

            bestWidth = formatWidth;
            bestHeight = formatHeight;

            bestFpsNumerator = fpsNumerator;
            bestFpsDenominator = fpsDenominator;
        }

        ++typeIndex;
    }

    // -----------------------------------------------------
    // Safety limit
    // -----------------------------------------------------

    if (typeIndex >= MAX_MEDIA_TYPES)
    {
        Logger::Log(
            "Camera: WARNING - Reached maximum media type "
            "enumeration limit of " +
            std::to_string(MAX_MEDIA_TYPES) +
            ".\n"
        );
    }

    // -----------------------------------------------------
    // Make sure we found an MJPG format
    // -----------------------------------------------------

    if (!bestType)
    {
        Logger::Log(
            "Camera: No suitable MJPG formats found.\n"
        );

        return false;
    }

    // -----------------------------------------------------
    // Log selected format
    // -----------------------------------------------------

    Logger::Log(
        "Camera: Selected MJPG format: " +
        std::to_string(bestWidth) +
        "x" +
        std::to_string(bestHeight) +
        " @ " +
        std::to_string(bestFpsNumerator) +
        "/" +
        std::to_string(bestFpsDenominator) +
        " FPS\n"
    );

    GUID subtype{};

    HRESULT hr2 = bestType->GetGUID(
        MF_MT_SUBTYPE,
        &subtype
    );

    if (SUCCEEDED(hr2))
    {
        wchar_t guidString[64]{};

        StringFromGUID2(
            subtype,
            guidString,
            ARRAYSIZE(guidString)
        );

        Logger::Log(
            "Camera subtype GUID: " +
            std::string(
                guidString,
                guidString + wcslen(guidString)
            ) +
            "\n"
        );
    }
    else
    {
        Logger::Log(
            "Failed to get camera subtype. HRESULT=0x" +
            std::to_string(
                static_cast<unsigned long>(hr2)
            ) +
            "\n"
        );
    }

    // -----------------------------------------------------
    // Set the NATIVE MJPG format
    //
    // IMPORTANT:
    //
    // We do NOT request RGB32.
    //
    // The Source Reader will therefore return the actual
    // compressed MJPG/JPEG frame from the camera.
    //
    // UploadFrame() is responsible for decoding it.
    // -----------------------------------------------------

    hr = m_reader->SetCurrentMediaType(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        nullptr,
        bestType.Get()
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: Failed to set MJPG format. "
            "HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        return false;
    }

    // -----------------------------------------------------
    // Get actual format selected by Media Foundation
    // -----------------------------------------------------

    Microsoft::WRL::ComPtr<IMFMediaType> actualType;

    hr = m_reader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        &actualType
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: Failed to get actual MJPG format.\n"
        );

        return false;
    }

    UINT32 actualWidth = 0;
    UINT32 actualHeight = 0;

    hr = MFGetAttributeSize(
        actualType.Get(),
        MF_MT_FRAME_SIZE,
        &actualWidth,
        &actualHeight
    );

    if (FAILED(hr))
        return false;

    UINT32 actualFpsNumerator = 0;
    UINT32 actualFpsDenominator = 1;

    hr = MFGetAttributeRatio(
        actualType.Get(),
        MF_MT_FRAME_RATE,
        &actualFpsNumerator,
        &actualFpsDenominator
    );

    if (FAILED(hr))
        return false;

    GUID actualSubtype{};

    hr = actualType->GetGUID(
        MF_MT_SUBTYPE,
        &actualSubtype
    );

    if (FAILED(hr))
        return false;

    // -----------------------------------------------------
    // Verify that Media Foundation actually accepted MJPG
    // -----------------------------------------------------

    if (actualSubtype != MFVideoFormat_MJPG)
    {
        Logger::Log(
            "Camera: ERROR - Media Foundation did not "
            "accept MJPG output.\n"
        );

        return false;
    }

    // -----------------------------------------------------
    // Store actual camera properties
    // -----------------------------------------------------

    m_width =
        static_cast<int>(actualWidth);

    m_height =
        static_cast<int>(actualHeight);

    m_fpsNumerator =
        actualFpsNumerator;

    m_fpsDenominator =
        actualFpsDenominator;

    // -----------------------------------------------------
    // Log final format
    // -----------------------------------------------------

    Logger::Log(
        "Camera: Actual format: " +
        std::to_string(m_width) +
        "x" +
        std::to_string(m_height) +
        " @ " +
        std::to_string(m_fpsNumerator) +
        "/" +
        std::to_string(m_fpsDenominator) +
        " FPS (MJPG)\n"
    );

    return true;
}




// =========================================================
// CreateTexture
// =========================================================

bool Camera::CreateTexture()
{
    if (!m_device)
        return false;

    if (m_width <= 0 || m_height <= 0)
        return false;

    // -----------------------------------------------------
    // Texture format
    //
    // WIC will decode the JPEG into BGRA.
    //
    // Therefore our D3D11 texture can directly use:
    //
    // DXGI_FORMAT_B8G8R8A8_UNORM
    // -----------------------------------------------------

    D3D11_TEXTURE2D_DESC description{};

    description.Width =
        static_cast<UINT>(m_width);

    description.Height =
        static_cast<UINT>(m_height);

    description.MipLevels = 1;

    description.ArraySize = 1;

    description.Format =
        DXGI_FORMAT_B8G8R8A8_UNORM;

    description.SampleDesc.Count = 1;

    description.SampleDesc.Quality = 0;

    description.Usage =
        D3D11_USAGE_DYNAMIC;

    description.BindFlags =
        D3D11_BIND_SHADER_RESOURCE;

    description.CPUAccessFlags =
        D3D11_CPU_ACCESS_WRITE;

    description.MiscFlags = 0;

    HRESULT hr = m_device->CreateTexture2D(
        &description,
        nullptr,
        &m_texture
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: Failed to create D3D11 texture. "
            "HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        return false;
    }

    // -----------------------------------------------------
    // Create shader resource view
    // -----------------------------------------------------

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDescription{};

    srvDescription.Format =
        DXGI_FORMAT_B8G8R8A8_UNORM;

    srvDescription.ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2D;

    srvDescription.Texture2D.MostDetailedMip = 0;

    srvDescription.Texture2D.MipLevels = 1;

    hr = m_device->CreateShaderResourceView(
        m_texture.Get(),
        &srvDescription,
        &m_shaderResourceView
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: Failed to create shader resource view. "
            "HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        m_texture.Reset();

        return false;
    }

    Logger::Log(
        "Camera: Created " +
        std::to_string(m_width) +
        "x" +
        std::to_string(m_height) +
        " BGRA D3D11 texture.\n"
    );

    return true;
}

// =========================================================
// CameraThread
//
// Runs entirely on the camera thread.
//
// This thread performs the expensive camera work:
//   - ReadSample()
//   - JPEG/MJPEG decoding
//   - WIC CopyPixels()
//
// It NEVER touches the D3D11 immediate context.
// =========================================================

// =========================================================
// CaptureFrame
//
// Runs on the camera thread.
//
// Reads one MJPG sample from Media Foundation and decodes
// it into BGRA pixels using WIC.
//
// The resulting BGRA frame is published to m_readyBuffer.
// =========================================================

bool Camera::CaptureFrame()
{
    if (!m_reader || !m_wicFactory)
        return false;

    // =====================================================
    // READ CAMERA SAMPLE
    // =====================================================

    DWORD streamIndex = 0;
    DWORD flags = 0;
    LONGLONG timestamp = 0;

    Microsoft::WRL::ComPtr<IMFSample> sample;

    HRESULT hr = m_reader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        &streamIndex,
        &flags,
        &timestamp,
        &sample
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: ReadSample failed. HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        return false;
    }

    if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
    {
        Logger::Log(
            "Camera: End of stream.\n"
        );

        return false;
    }

    if (flags & MF_SOURCE_READERF_STREAMTICK)
    {
        return false;
    }

    if (!sample)
        return false;


    // =====================================================
    // GET MEDIA BUFFER
    // =====================================================

    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;

    hr = sample->ConvertToContiguousBuffer(
        &buffer
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: ConvertToContiguousBuffer failed. "
            "HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        return false;
    }


    BYTE* data = nullptr;

    DWORD maxLength = 0;
    DWORD currentLength = 0;

    hr = buffer->Lock(
        &data,
        &maxLength,
        &currentLength
    );

    if (
        FAILED(hr) ||
        !data ||
        currentLength == 0
        )
    {
        Logger::Log(
            "Camera: Buffer Lock failed.\n"
        );

        if (SUCCEEDED(hr))
            buffer->Unlock();

        return false;
    }


    // =====================================================
    // CREATE WIC STREAM
    // =====================================================

    Microsoft::WRL::ComPtr<IWICStream> stream;

    hr = m_wicFactory->CreateStream(
        &stream
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: WIC CreateStream failed.\n"
        );

        buffer->Unlock();
        return false;
    }

    hr = stream->InitializeFromMemory(
        data,
        currentLength
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: WIC InitializeFromMemory failed.\n"
        );

        buffer->Unlock();
        return false;
    }


    // =====================================================
    // JPEG DECODER
    // =====================================================

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;

    hr = m_wicFactory->CreateDecoderFromStream(
        stream.Get(),
        nullptr,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: WIC JPEG decoder creation failed.\n"
        );

        buffer->Unlock();
        return false;
    }


    // =====================================================
    // GET FRAME
    // =====================================================

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;

    hr = decoder->GetFrame(
        0,
        &frame
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: WIC GetFrame failed.\n"
        );

        buffer->Unlock();
        return false;
    }


    // =====================================================
    // FORMAT CONVERTER
    // =====================================================

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;

    hr = m_wicFactory->CreateFormatConverter(
        &converter
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: WIC CreateFormatConverter failed.\n"
        );

        buffer->Unlock();
        return false;
    }

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: WIC converter initialization failed.\n"
        );

        buffer->Unlock();
        return false;
    }


    // =====================================================
    // GET IMAGE SIZE
    // =====================================================

    UINT decodedWidth = 0;
    UINT decodedHeight = 0;

    hr = converter->GetSize(
        &decodedWidth,
        &decodedHeight
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: WIC GetSize failed.\n"
        );

        buffer->Unlock();
        return false;
    }


    // =====================================================
    // PREPARE CAMERA BUFFER
    // =====================================================

    const size_t rowStride =
        static_cast<size_t>(decodedWidth) * 4;

    const size_t bufferSize =
        rowStride *
        static_cast<size_t>(decodedHeight);

    try
    {
        if (m_captureBuffer.size() != bufferSize)
        {
            m_captureBuffer.resize(bufferSize);
        }
    }
    catch (...)
    {
        Logger::Log(
            "Camera: Failed to resize capture buffer.\n"
        );

        buffer->Unlock();
        return false;
    }


    // =====================================================
    // WIC COPY PIXELS
    //
    // THIS IS ONE OF THE EXPENSIVE OPERATIONS.
    //
    // It happens entirely on the camera thread.
    // =====================================================

    hr = converter->CopyPixels(
        nullptr,
        static_cast<UINT>(rowStride),
        static_cast<UINT>(bufferSize),
        m_captureBuffer.data()
    );

    if (FAILED(hr))
    {
        Logger::Log(
            "Camera: WIC CopyPixels failed. HRESULT: 0x" +
            std::to_string(
                static_cast<unsigned long>(hr)
            ) +
            "\n"
        );

        buffer->Unlock();
        return false;
    }


    // =====================================================
    // DONE WITH MEDIA BUFFER
    // =====================================================

    buffer->Unlock();


    // =====================================================
    // PUBLISH FRAME
    //
    // Only the vector swap is protected by the mutex.
    //
    // The expensive decoding above happened completely
    // outside the mutex.
    // =====================================================

    {
        std::lock_guard<std::mutex> lock(
            m_frameMutex
        );

        std::swap(
            m_captureBuffer,
            m_readyBuffer
        );

        m_newFrameAvailable.store(
            true,
            std::memory_order_release
        );
    }

    return true;
}

void Camera::CameraThread()
{
    while (m_running.load(std::memory_order_acquire))
    {
        if (!CaptureFrame())
        {
            // Don't busy-spin if the camera temporarily
            // doesn't provide a frame.
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1)
            );
        }
    }
}


// =========================================================
// Update
// =========================================================

bool Camera::Update()
{
    if (!m_open)
        return false;

    if (!m_newFrameAvailable.load(
        std::memory_order_acquire))
    {
        return false;
    }

    std::vector<BYTE> frame;

    {
        std::lock_guard<std::mutex> lock(
            m_frameMutex
        );

        if (m_readyBuffer.empty())
        {
            m_newFrameAvailable.store(
                false,
                std::memory_order_release
            );

            return false;
        }

        frame.swap(m_readyBuffer);

        m_newFrameAvailable.store(
            false,
            std::memory_order_release
        );
    }

    return UploadFrame(
        frame.data(),
        frame.size()
    );
}


// =========================================================
// UploadFrame
//
// MJPG sample:
//
//     JPEG compressed bytes
//             |
//             v
//       WIC JPEG decoder
//             |
//             v
//       BGRA 32-bit pixels
//             |
//             v
//       D3D11 texture
// =========================================================

bool Camera::UploadFrame(
    const BYTE* pixels,
    size_t pixelSize
)
{
    if (
        !pixels ||
        pixelSize == 0 ||
        !m_texture ||
        !m_context
        )
    {
        Logger::Log(
            "UploadFrame: INVALID INPUT\n"
        );

        return false;
    }

    const size_t expectedSize =
        static_cast<size_t>(m_width) *
        static_cast<size_t>(m_height) *
        4;

    if (pixelSize < expectedSize)
    {
        Logger::Log(
            "UploadFrame: Pixel buffer too small\n"
        );

        return false;
    }

    // ---------------------------------------------------------
    // MAP D3D11 TEXTURE
    // ---------------------------------------------------------

    D3D11_MAPPED_SUBRESOURCE mapped{};

    HRESULT hr =
        m_context->Map(
            m_texture.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped
        );

    if (FAILED(hr))
    {
        Logger::Log(
            "UploadFrame: D3D11 Map FAILED\n"
        );

        return false;
    }

    // ---------------------------------------------------------
    // COPY BGRA PIXELS INTO TEXTURE
    // ---------------------------------------------------------

    const size_t rowBytes =
        static_cast<size_t>(m_width) * 4;

    BYTE* destination =
        static_cast<BYTE*>(mapped.pData);

    for (int y = 0; y < m_height; ++y)
    {
        std::memcpy(
            destination +
            static_cast<size_t>(y) *
            mapped.RowPitch,

            pixels +
            static_cast<size_t>(y) *
            rowBytes,

            rowBytes
        );
    }

    // ---------------------------------------------------------
    // UNMAP
    // ---------------------------------------------------------

    m_context->Unmap(
        m_texture.Get(),
        0
    );

    return true;
}


// =========================================================
// GetTexture
// =========================================================

ID3D11ShaderResourceView*
Camera::GetTexture() const
{
    return m_shaderResourceView.Get();
}


// =========================================================
// GetWidth
// =========================================================

int Camera::GetWidth() const
{
    return m_width;
}


// =========================================================
// GetHeight
// =========================================================

int Camera::GetHeight() const
{
    return m_height;
}


// =========================================================
// IsOpen
// =========================================================

bool Camera::IsOpen() const
{
    return m_open;
}


// =========================================================
// Shutdown
// =========================================================

void Camera::Shutdown()
{
    // -----------------------------------------------------
    // Tell camera thread to stop
    // -----------------------------------------------------

    m_open.store(
        false,
        std::memory_order_release
    );

    m_running.store(
        false,
        std::memory_order_release
    );


    // -----------------------------------------------------
    // Wait for camera thread to finish
    //
    // IMPORTANT:
    // Do this BEFORE releasing m_reader or m_wicFactory.
    // -----------------------------------------------------

    if (m_cameraThread.joinable())
    {
        m_cameraThread.join();
    }


    // -----------------------------------------------------
    // Release D3D11 resources
    //
    // These are owned/used by the render thread.
    // -----------------------------------------------------

    m_shaderResourceView.Reset();

    m_texture.Reset();


    // -----------------------------------------------------
    // Release Media Foundation reader
    // -----------------------------------------------------

    m_reader.Reset();


    // -----------------------------------------------------
    // Release WIC
    // -----------------------------------------------------

    m_wicFactory.Reset();


    // -----------------------------------------------------
    // Clear frame buffers
    // -----------------------------------------------------

    {
        std::lock_guard<std::mutex> lock(
            m_frameMutex
        );

        m_captureBuffer.clear();
        m_readyBuffer.clear();

        m_newFrameAvailable.store(
            false,
            std::memory_order_release
        );
    }


    // -----------------------------------------------------
    // Reset camera state
    // -----------------------------------------------------

    m_width = 0;
    m_height = 0;

    m_fpsNumerator = 0;
    m_fpsDenominator = 1;

    m_device = nullptr;
    m_context = nullptr;


    // -----------------------------------------------------
    // Shutdown Media Foundation
    // -----------------------------------------------------

    if (m_mediaFoundationStarted)
    {
        MFShutdown();

        m_mediaFoundationStarted = false;
    }
}