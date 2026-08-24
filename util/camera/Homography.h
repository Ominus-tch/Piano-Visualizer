#pragma once

struct Homography
{
    float h[3][3];
};

Homography CalculateHomography(
    const float src[4][2],
    const float dst[4][2]
);

void TransformPoint(
    const Homography& H,
    float x,
    float y,
    float& outX,
    float& outY
);

// Creates a virtual rectangle whose bottom edge is:
//
// bottomLeft -> bottomRight
//
// and whose top edge follows the perspective direction
// established by the physical piano's side edges.
void BuildVirtualPianoCorners(
    const float topLeft[2],
    const float bottomLeft[2],
    const float bottomRight[2],
    const float topRight[2],
    float heightScale,
    float outCorners[4][2]
);