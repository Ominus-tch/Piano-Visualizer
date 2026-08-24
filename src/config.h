#pragma once

#include <imgui/imgui.h>

#include <nlohmann/json.hpp>
#include <fstream>
using json = nlohmann::json;

// ============================================================
// PIANO CONFIGURATION
// ============================================================

namespace Config {

    static const char* pianoConfigFile = "piano_config.json";

    bool SavePianoConfig(
        const std::vector<ImVec2>& polygonPoints,
        float horizontalFovDegrees,
        float planeWidth,
        float planeDepth,
        float surfaceXOffset,
        float surfaceYOffset,
        float surfaceZOffset
    )
    {
        if (polygonPoints.size() != 4)
            return false;

        json config;

        // --------------------------------------------------------
        // Selected polygon points
        // --------------------------------------------------------

        config["points"]["P1"]["x"] = polygonPoints[0].x;
        config["points"]["P1"]["y"] = polygonPoints[0].y;

        config["points"]["P2"]["x"] = polygonPoints[1].x;
        config["points"]["P2"]["y"] = polygonPoints[1].y;

        config["points"]["P3"]["x"] = polygonPoints[2].x;
        config["points"]["P3"]["y"] = polygonPoints[2].y;

        config["points"]["P4"]["x"] = polygonPoints[3].x;
        config["points"]["P4"]["y"] = polygonPoints[3].y;

        // --------------------------------------------------------
        // Camera / plane settings
        // --------------------------------------------------------

        config["settings"]["horizontalFovDegrees"] =
            horizontalFovDegrees;

        config["settings"]["planeWidth"] =
            planeWidth;

        config["settings"]["planeDepth"] =
            planeDepth;

        config["settings"]["surfaceXOffset"] =
            surfaceXOffset;

        config["settings"]["surfaceYOffset"] =
            surfaceYOffset;

        config["settings"]["surfaceZOffset"] =
            surfaceZOffset;

        // --------------------------------------------------------
        // Save
        // --------------------------------------------------------

        std::ofstream file(pianoConfigFile);

        if (!file.is_open())
            return false;

        file << config.dump(4);

        file.close();

        return true;
    }

    bool LoadPianoConfig(
        std::vector<ImVec2>& polygonPoints,
        float& horizontalFovDegrees,
        float& planeWidth,
        float& planeDepth,
        float& surfaceXOffset,
        float& surfaceYOffset,
        float& surfaceZOffset
    )
    {
        std::ifstream file(pianoConfigFile);

        if (!file.is_open())
            return false;

        try
        {
            json config;
            file >> config;

            // ----------------------------------------------------
            // Make sure all points exist
            // ----------------------------------------------------

            if (!config.contains("points"))
                return false;

            if (!config["points"].contains("P1") ||
                !config["points"].contains("P2") ||
                !config["points"].contains("P3") ||
                !config["points"].contains("P4"))
            {
                return false;
            }

            // ----------------------------------------------------
            // Load points
            // ----------------------------------------------------

            polygonPoints.resize(4);

            polygonPoints[0] = ImVec2(
                config["points"]["P1"]["x"].get<float>(),
                config["points"]["P1"]["y"].get<float>()
            );

            polygonPoints[1] = ImVec2(
                config["points"]["P2"]["x"].get<float>(),
                config["points"]["P2"]["y"].get<float>()
            );

            polygonPoints[2] = ImVec2(
                config["points"]["P3"]["x"].get<float>(),
                config["points"]["P3"]["y"].get<float>()
            );

            polygonPoints[3] = ImVec2(
                config["points"]["P4"]["x"].get<float>(),
                config["points"]["P4"]["y"].get<float>()
            );

            // ----------------------------------------------------
            // Load settings
            // ----------------------------------------------------

            if (config.contains("settings"))
            {
                auto& settings = config["settings"];

                if (settings.contains("horizontalFovDegrees"))
                {
                    horizontalFovDegrees =
                        settings["horizontalFovDegrees"].get<float>();
                }

                if (settings.contains("planeWidth"))
                {
                    planeWidth =
                        settings["planeWidth"].get<float>();
                }

                if (settings.contains("planeDepth"))
                {
                    planeDepth =
                        settings["planeDepth"].get<float>();
                }

                if (settings.contains("surfaceXOffset"))
                {
                    surfaceXOffset =
                        settings["surfaceXOffset"].get<float>();
                }

                if (settings.contains("surfaceYOffset"))
                {
                    surfaceYOffset =
                        settings["surfaceYOffset"].get<float>();
                }

                if (settings.contains("surfaceZOffset"))
                {
                    surfaceZOffset =
                        settings["surfaceZOffset"].get<float>();
                }
            }

            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
}