#include "VirtualWindowRenderer.h"

#include <cmath>
#include <algorithm>


// =============================================================
// Small 3x3 matrix solver
//
// Solves:
//
//     A * x = b
//
// using Gaussian elimination.
// =============================================================

static bool Solve8x8(
    double A[8][8],
    double b[8],
    double x[8]
)
{
    for (int i = 0; i < 8; ++i)
    {
        // -----------------------------------------------------
        // Find pivot
        // -----------------------------------------------------

        int pivot = i;

        double maxValue =
            std::abs(A[i][i]);

        for (int r = i + 1; r < 8; ++r)
        {
            double value =
                std::abs(A[r][i]);

            if (value > maxValue)
            {
                maxValue = value;
                pivot = r;
            }
        }

        if (maxValue < 1e-12)
            return false;


        // -----------------------------------------------------
        // Swap rows
        // -----------------------------------------------------

        if (pivot != i)
        {
            for (int c = i; c < 8; ++c)
            {
                std::swap(
                    A[i][c],
                    A[pivot][c]
                );
            }

            std::swap(
                b[i],
                b[pivot]
            );
        }


        // -----------------------------------------------------
        // Normalize pivot row
        // -----------------------------------------------------

        const double divisor =
            A[i][i];

        for (int c = i; c < 8; ++c)
        {
            A[i][c] /= divisor;
        }

        b[i] /= divisor;


        // -----------------------------------------------------
        // Eliminate column
        // -----------------------------------------------------

        for (int r = 0; r < 8; ++r)
        {
            if (r == i)
                continue;

            const double factor =
                A[r][i];

            if (std::abs(factor) < 1e-12)
                continue;

            for (int c = i; c < 8; ++c)
            {
                A[r][c] -=
                    factor * A[i][c];
            }

            b[r] -=
                factor * b[i];
        }
    }


    for (int i = 0; i < 8; ++i)
    {
        x[i] = b[i];
    }

    return true;
}


// =============================================================
// HOMOGRAPHY
//
// Maps:
//
//     (u,v)
//
// to:
//
//     (screenX, screenY)
//
//
//
// Source:
//
//     (0,0) ---------------- (1,0)
//       |                       |
//       |                       |
//       |                       |
//     (0,1) ---------------- (1,1)
//
// Destination:
//
//     topLeft ---------------- topRight
//       |                         |
//       |                         |
//       |                         |
//     P1/bottomLeft -------- P4/bottomRight
//
// =============================================================

struct Homography
{
    double h11;
    double h12;
    double h13;

    double h21;
    double h22;
    double h23;

    double h31;
    double h32;

    bool valid = false;


    ImVec2 Project(
        float u,
        float v
    ) const
    {
        const double denominator =
            h31 * u +
            h32 * v +
            1.0;

        if (std::abs(denominator) < 1e-12)
        {
            return ImVec2(
                0.0f,
                0.0f
            );
        }


        const double x =
            (
                h11 * u +
                h12 * v +
                h13
                ) / denominator;


        const double y =
            (
                h21 * u +
                h22 * v +
                h23
                ) / denominator;


        return ImVec2(
            static_cast<float>(x),
            static_cast<float>(y)
        );
    }
};


// =============================================================
// CALCULATE HOMOGRAPHY
// =============================================================

static Homography CalculateHomography(
    const ImVec2 src[4],
    const ImVec2 dst[4]
)
{
    Homography result{};


    double A[8][8] = {};
    double b[8] = {};


    for (int i = 0; i < 4; ++i)
    {
        const double x =
            src[i].x;

        const double y =
            src[i].y;

        const double X =
            dst[i].x;

        const double Y =
            dst[i].y;


        const int row =
            i * 2;


        // -----------------------------------------------------
        // X equation
        // -----------------------------------------------------

        A[row][0] = x;
        A[row][1] = y;
        A[row][2] = 1.0;

        A[row][3] = 0.0;
        A[row][4] = 0.0;
        A[row][5] = 0.0;

        A[row][6] =
            -X * x;

        A[row][7] =
            -X * y;

        b[row] = X;


        // -----------------------------------------------------
        // Y equation
        // -----------------------------------------------------

        A[row + 1][0] = 0.0;
        A[row + 1][1] = 0.0;
        A[row + 1][2] = 0.0;

        A[row + 1][3] = x;
        A[row + 1][4] = y;
        A[row + 1][5] = 1.0;

        A[row + 1][6] =
            -Y * x;

        A[row + 1][7] =
            -Y * y;

        b[row + 1] = Y;
    }


    double x[8] = {};


    if (!Solve8x8(
        A,
        b,
        x
    ))
    {
        return result;
    }


    result.h11 = x[0];
    result.h12 = x[1];
    result.h13 = x[2];

    result.h21 = x[3];
    result.h22 = x[4];
    result.h23 = x[5];

    result.h31 = x[6];
    result.h32 = x[7];

    result.valid = true;


    return result;
}


// =============================================================
// RENDER
// =============================================================

void VirtualWindowRenderer::Render(
    ImDrawList* drawList,
    ImTextureID texture,

    const ImVec2& topLeft,
    const ImVec2& topRight,
    const ImVec2& bottomRight,
    const ImVec2& bottomLeft,

    const Settings& settings
)
{
    if (!drawList)
        return;

    if (!texture)
        return;


    const int gridX =
        settings.gridX;

    const int gridY =
        settings.gridY;


    if (gridX < 1 ||
        gridY < 1)
    {
        return;
    }


    // =========================================================
    // SOURCE RECTANGLE
    //
    // This is the captured window.
    //
    // topLeft     = (0,0)
    // topRight    = (1,0)
    // bottomRight = (1,1)
    // bottomLeft  = (0,1)
    // =========================================================

    const ImVec2 source[4] =
    {
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 0.0f),
        ImVec2(1.0f, 1.0f),
        ImVec2(0.0f, 1.0f)
    };


    // =========================================================
    // DESTINATION
    //
    // IMPORTANT:
    //
    // Your physical keyboard is:
    //
    //     P1 ---------------- P4
    //      |                    |
    //      |                    |
    //     P2 ---------------- P3
    //
    //
    // But the virtual window has:
    //
    //     topLeft -------- topRight
    //        |                 |
    //        |                 |
    //     P1 ---------------- P4
    //
    // Therefore:
    //
    // bottomLeft  = P1
    // bottomRight = P4
    //
    // exactly as you described.
    // =========================================================

    const ImVec2 destination[4] =
    {
        topLeft,
        topRight,
        bottomRight,
        bottomLeft
    };


    // =========================================================
    // CALCULATE PROJECTIVE TRANSFORMATION
    // =========================================================

    const Homography H =
        CalculateHomography(
            source,
            destination
        );


    if (!H.valid)
        return;


    // =========================================================
    // DRAW PERSPECTIVE-WARPED TEXTURE
    // =========================================================

    for (int y = 0; y < gridY; ++y)
    {
        for (int x = 0; x < gridX; ++x)
        {
            const float u0 =
                static_cast<float>(x) /
                static_cast<float>(gridX);

            const float u1 =
                static_cast<float>(x + 1) /
                static_cast<float>(gridX);


            const float v0 =
                static_cast<float>(y) /
                static_cast<float>(gridY);

            const float v1 =
                static_cast<float>(y + 1) /
                static_cast<float>(gridY);


            // -------------------------------------------------
            // PROJECT EACH CORNER
            // -------------------------------------------------

            const ImVec2 p00 =
                H.Project(
                    u0,
                    v0
                );


            const ImVec2 p10 =
                H.Project(
                    u1,
                    v0
                );


            const ImVec2 p11 =
                H.Project(
                    u1,
                    v1
                );


            const ImVec2 p01 =
                H.Project(
                    u0,
                    v1
                );


            // -------------------------------------------------
            // UVs
            //
            // No flipping.
            //
            // Source and destination have the same orientation.
            // -------------------------------------------------

            const ImVec2 uv00(
                u0,
                v0
            );

            const ImVec2 uv10(
                u1,
                v0
            );

            const ImVec2 uv11(
                u1,
                v1
            );

            const ImVec2 uv01(
                u0,
                v1
            );


            // -------------------------------------------------
            // DRAW
            // -------------------------------------------------

            drawList->AddImageQuad(
                texture,

                p00,
                p10,
                p11,
                p01,

                uv00,
                uv10,
                uv11,
                uv01,

                settings.tint
            );
        }
    }

    if (settings.drawDebugLines) {
        // =========================================================
        // DEBUG OUTLINE
        // =========================================================

        drawList->AddLine(
            topLeft,
            topRight,
            IM_COL32(
                0,
                150,
                255,
                230
            ),
            2.0f
        );


        drawList->AddLine(
            topRight,
            bottomRight,
            IM_COL32(
                0,
                150,
                255,
                230
            ),
            2.0f
        );


        drawList->AddLine(
            bottomRight,
            bottomLeft,
            IM_COL32(
                0,
                150,
                255,
                230
            ),
            2.0f
        );


        drawList->AddLine(
            bottomLeft,
            topLeft,
            IM_COL32(
                0,
                150,
                255,
                230
            ),
            2.0f
        );
    }
}