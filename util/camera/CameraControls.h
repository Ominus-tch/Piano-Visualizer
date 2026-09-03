#pragma once

#include <windows.h>

#include <mfidl.h>

#include <ks.h>
#include <ksmedia.h>
#include <ksproxy.h>

#include <wrl/client.h>

class CameraControls
{
public:
    CameraControls() = default;
    ~CameraControls();

    CameraControls(
        const CameraControls&
    ) = delete;

    CameraControls& operator=(
        const CameraControls&
        ) = delete;

    bool Initialize(
        IMFMediaSource* source
    );

    void Shutdown();

    bool IsInitialized() const;

    // =========================================================
    // Exposure
    // =========================================================

    bool GetAutoExposure(
        bool& enabled
    ) const;

    bool SetAutoExposure(
        bool enabled
    );

    bool GetExposure(
        int& exposure
    ) const;

    bool SetExposure(
        int exposure
    );

    bool GetExposureRange(
        int& minimum,
        int& maximum,
        int& step
    );

    // =========================================================
    // Low light compensation
    // =========================================================

    bool GetLowLightCompensation(
        bool& enabled
    ) const;

    bool SetLowLightCompensation(
        bool enabled
    );

    // =========================================================
    // Focus
    // =========================================================

    bool GetAutoFocus(
        bool& enabled
    ) const;

    bool SetAutoFocus(
        bool enabled
    );

    bool GetFocus(
        int& focus
    ) const;

    bool SetFocus(
        int focus
    );

    bool GetFocusRange(
        int& minimum,
        int& maximum,
        int& step
    );

	// =========================================================
    // Image Settings
	// =========================================================

    bool GetVideoProcAmpProperty(
        ULONG propertyId,
        int& value
    ) const;

    bool SetVideoProcAmpProperty(
        ULONG propertyId,
        int value
    );

    bool GetVideoProcAmpRange(
        ULONG propertyId,
        int& minimum,
        int& maximum,
        int& step
    );

    bool GetBrightness(int& value) const;
    bool SetBrightness(int value);
    bool GetBrightnessRange(int& minimum, int& maximum, int& step);

    bool GetContrast(int& value) const;
    bool SetContrast(int value);
    bool GetContrastRange(int& minimum, int& maximum, int& step);

    bool GetSaturation(int& value) const;
    bool SetSaturation(int value);
    bool GetSaturationRange(int& minimum, int& maximum, int& step);

    bool GetSharpness(int& value) const;
    bool SetSharpness(int value);
    bool GetSharpnessRange(int& minimum, int& maximum, int& step);

private:
    bool GetProperty(
        ULONG propertyId,
        ULONG& value
    ) const;

    bool SetProperty(
        ULONG propertyId,
        ULONG value
    );

private:
    Microsoft::WRL::ComPtr<IKsControl>
        m_ksControl;

    int m_exposureMin = 0;
    int m_exposureMax = 0;
    int m_exposureStep = 1;

    int m_focusMin = 0;
    int m_focusMax = 0;
    int m_focusStep = 1;

    int m_brightnessMin = 0;
    int m_brightnessMax = 0;
    int m_brightnessStep = 1;

    int m_contrastMin = 0;
    int m_contrastMax = 0;
    int m_contrastStep = 1;

    int m_saturationMin = 0;
    int m_saturationMax = 0;
    int m_saturationStep = 1;

    int m_sharpnessMin = 0;
    int m_sharpnessMax = 0;
    int m_sharpnessStep = 1;
};