#include "Homography.h"

#include <cmath>
#include <algorithm>


// ============================================================
// Solve an 8x8 linear system using Gaussian elimination
// ============================================================

static bool SolveLinearSystem(
    float A[8][9],
    float result[8]
)
{
    for (int i = 0; i < 8; ++i)
    {
        // ----------------------------------------------------
        // Find pivot row
        // ----------------------------------------------------

        int pivotRow = i;

        float largest =
            std::fabs(A[i][i]);

        for (int row = i + 1; row < 8; ++row)
        {
            float value =
                std::fabs(A[row][i]);

            if (value > largest)
            {
                largest = value;
                pivotRow = row;
            }
        }

        // ----------------------------------------------------
        // Matrix is singular
        // ----------------------------------------------------

        if (largest < 0.000001f)
        {
            return false;
        }

        // ----------------------------------------------------
        // Swap rows
        // ----------------------------------------------------

        if (pivotRow != i)
        {
            for (int column = i; column < 9; ++column)
            {
                std::swap(
                    A[i][column],
                    A[pivotRow][column]
                );
            }
        }

        // ----------------------------------------------------
        // Normalize pivot row
        // ----------------------------------------------------

        float pivot =
            A[i][i];

        for (int column = i; column < 9; ++column)
        {
            A[i][column] /= pivot;
        }

        // ----------------------------------------------------
        // Eliminate this column from all other rows
        // ----------------------------------------------------

        for (int row = 0; row < 8; ++row)
        {
            if (row == i)
            {
                continue;
            }

            float factor =
                A[row][i];

            if (std::fabs(factor) < 0.000001f)
            {
                continue;
            }

            for (int column = i; column < 9; ++column)
            {
                A[row][column] -=
                    factor *
                    A[i][column];
            }
        }
    }

    // --------------------------------------------------------
    // Extract solution
    // --------------------------------------------------------

    for (int i = 0; i < 8; ++i)
    {
        result[i] = A[i][8];
    }

    return true;
}


// ============================================================
// Calculate Homography
// ============================================================

Homography CalculateHomography(
    const float src[4][2],
    const float dst[4][2]
)
{
    Homography H{};

    // --------------------------------------------------------
    // We solve for:
    //
    // h11 h12 h13
    // h21 h22 h23
    // h31 h32  1
    //
    // The final element is fixed to 1.
    //
    // This leaves 8 unknowns.
    // --------------------------------------------------------

    float A[8][9] = {};

    int row = 0;

    for (int i = 0; i < 4; ++i)
    {
        float x = src[i][0];
        float y = src[i][1];

        float X = dst[i][0];
        float Y = dst[i][1];

        // ----------------------------------------------------
        // Equation for X
        //
        // X = (h11*x + h12*y + h13) /
        //     (h31*x + h32*y + 1)
        // ----------------------------------------------------

        A[row][0] = x;
        A[row][1] = y;
        A[row][2] = 1.0f;

        A[row][3] = 0.0f;
        A[row][4] = 0.0f;
        A[row][5] = 0.0f;

        A[row][6] = -X * x;
        A[row][7] = -X * y;

        A[row][8] = X;

        ++row;

        // ----------------------------------------------------
        // Equation for Y
        // ----------------------------------------------------

        A[row][0] = 0.0f;
        A[row][1] = 0.0f;
        A[row][2] = 0.0f;

        A[row][3] = x;
        A[row][4] = y;
        A[row][5] = 1.0f;

        A[row][6] = -Y * x;
        A[row][7] = -Y * y;

        A[row][8] = Y;

        ++row;
    }

    // --------------------------------------------------------
    // Solve
    // --------------------------------------------------------

    float solution[8] = {};

    if (!SolveLinearSystem(A, solution))
    {
        // ----------------------------------------------------
        // Return identity matrix if the points are invalid
        // or the system cannot be solved.
        // ----------------------------------------------------

        H.h[0][0] = 1.0f;
        H.h[0][1] = 0.0f;
        H.h[0][2] = 0.0f;

        H.h[1][0] = 0.0f;
        H.h[1][1] = 1.0f;
        H.h[1][2] = 0.0f;

        H.h[2][0] = 0.0f;
        H.h[2][1] = 0.0f;
        H.h[2][2] = 1.0f;

        return H;
    }

    // --------------------------------------------------------
    // Build matrix
    // --------------------------------------------------------

    H.h[0][0] = solution[0];
    H.h[0][1] = solution[1];
    H.h[0][2] = solution[2];

    H.h[1][0] = solution[3];
    H.h[1][1] = solution[4];
    H.h[1][2] = solution[5];

    H.h[2][0] = solution[6];
    H.h[2][1] = solution[7];
    H.h[2][2] = 1.0f;

    return H;
}


// ============================================================
// Transform Point
// ============================================================

void TransformPoint(
    const Homography& H,
    float x,
    float y,
    float& outX,
    float& outY
)
{
    // --------------------------------------------------------
    // Homogeneous transformation:
    //
    // [X]   [h00 h01 h02] [x]
    // [Y] = [h10 h11 h12] [y]
    // [W]   [h20 h21 h22] [1]
    //
    // Final coordinates:
    //
    // x' = X / W
    // y' = Y / W
    // --------------------------------------------------------

    float X =
        H.h[0][0] * x +
        H.h[0][1] * y +
        H.h[0][2];

    float Y =
        H.h[1][0] * x +
        H.h[1][1] * y +
        H.h[1][2];

    float W =
        H.h[2][0] * x +
        H.h[2][1] * y +
        H.h[2][2];

    // --------------------------------------------------------
    // Avoid division by zero
    // --------------------------------------------------------

    if (std::fabs(W) < 0.000001f)
    {
        outX = 0.0f;
        outY = 0.0f;
        return;
    }

    outX = X / W;
    outY = Y / W;
}

// ============================================================
// Build Virtual Piano Corners
// ============================================================
//
// Input:
//
//     physicalTopLeft      = P0
//     physicalBottomLeft   = P1
//     physicalBottomRight  = P2
//     physicalTopRight     = P3
//
// The virtual piano is anchored to:
//
//     P1 ---------------- P2
//       bottom edge
//
// The top edge is generated by extending the two physical
// side directions:
//
//     P1 -> P0
//     P2 -> P3
//
// heightScale controls how tall the virtual piano is.
//
//     1.0 = same depth as the selected piano area
//     0.5 = half as deep
//     2.0 = twice as deep
//
// Output order:
//
//     0 = top-left
//     1 = bottom-left
//     2 = bottom-right
//     3 = top-right
//
// ============================================================

void BuildVirtualPianoCorners(
    const float topLeft[2],
    const float bottomLeft[2],
    const float bottomRight[2],
    const float topRight[2],
    float heightScale,
    float outCorners[4][2]
)
{
    // --------------------------------------------------------
    // The virtual piano is anchored to:
    //
    // P1 ---------------- P4
    //
    // which is the TOP edge of the selected piano.
    //
    // P2 and P3 are used to determine the direction in which
    // the virtual piano extends.
    // --------------------------------------------------------


    // --------------------------------------------------------
    // Left direction:
    //
    // P1 -> P2
    // --------------------------------------------------------

    float leftDirectionX =
        bottomLeft[0] - topLeft[0];

    float leftDirectionY =
        bottomLeft[1] - topLeft[1];


    // --------------------------------------------------------
    // Right direction:
    //
    // P4 -> P3
    // --------------------------------------------------------

    float rightDirectionX =
        bottomRight[0] - topRight[0];

    float rightDirectionY =
        bottomRight[1] - topRight[1];


    // --------------------------------------------------------
    // Bottom of virtual piano = P1 -> P4
    // --------------------------------------------------------

    outCorners[0][0] =
        topLeft[0];

    outCorners[0][1] =
        topLeft[1];


    outCorners[3][0] =
        topRight[0];

    outCorners[3][1] =
        topRight[1];


    // --------------------------------------------------------
    // Generate the opposite edge by extending from P1/P4
    // toward P2/P3.
    // --------------------------------------------------------

    outCorners[1][0] =
        topLeft[0] +
        leftDirectionX * heightScale;

    outCorners[1][1] =
        topLeft[1] +
        leftDirectionY * heightScale;


    outCorners[2][0] =
        topRight[0] +
        rightDirectionX * heightScale;

    outCorners[2][1] =
        topRight[1] +
        rightDirectionY * heightScale;
}