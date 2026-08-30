#pragma once

#include <Windows.h>
#include <oleidl.h>

#include <filesystem>
#include <string>
#include <vector>

class FileDropTarget : public IDropTarget
{
public:
    explicit FileDropTarget(
        std::vector<std::wstring> extensions = {});

    ~FileDropTarget() = default;

    // ------------------------------------------------------------
    // Register / unregister
    // ------------------------------------------------------------

    bool Register(HWND window);

    void Unregister();

    // ------------------------------------------------------------
    // State
    // ------------------------------------------------------------

    bool isDragging() const;

    bool hasValidFile() const;

    std::vector<std::filesystem::path> consumeDroppedFiles();

    // ------------------------------------------------------------
    // IDropTarget
    // ------------------------------------------------------------

    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID riid,
        void** ppvObject) override;

    ULONG STDMETHODCALLTYPE AddRef() override;

    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE DragEnter(
        IDataObject* dataObject,
        DWORD keyState,
        POINTL point,
        DWORD* effect) override;

    HRESULT STDMETHODCALLTYPE DragOver(
        DWORD keyState,
        POINTL point,
        DWORD* effect) override;

    HRESULT STDMETHODCALLTYPE DragLeave() override;

    HRESULT STDMETHODCALLTYPE Drop(
        IDataObject* dataObject,
        DWORD keyState,
        POINTL point,
        DWORD* effect) override;

private:
    bool extractFiles(
        IDataObject* dataObject,
        std::vector<std::filesystem::path>& files) const;

    bool isValidFile(
        const std::filesystem::path& path) const;

    void updateDragState(
        IDataObject* dataObject);

private:
    HWND _window = nullptr;

    std::vector<std::wstring> _extensions;

    bool _registered = false;
    bool _dragging = false;
    bool _hasValidFile = false;

    std::vector<std::filesystem::path> _droppedFiles;
};