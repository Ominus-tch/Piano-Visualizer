#pragma once

#include <imgui/imgui.h>

class VirtualWindowRenderer
{
public:

    struct Settings
    {
        int gridX = 64;
        int gridY = 36;

        bool drawDebugLines = false;

        ImU32 tint =
            IM_COL32(
                255,
                255,
                255,
                255
            );
    };


    void Render(
        ImDrawList* drawList,
        ImTextureID texture,

        const ImVec2& topLeft,
        const ImVec2& topRight,
        const ImVec2& bottomRight,
        const ImVec2& bottomLeft,

        const Settings& settings
    );


private:

    ImVec2 Bilinear(
        const ImVec2& topLeft,
        const ImVec2& topRight,
        const ImVec2& bottomRight,
        const ImVec2& bottomLeft,

        float u,
        float v
    );
};