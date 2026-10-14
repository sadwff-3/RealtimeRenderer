#version 450 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D currentColor;
uniform sampler2D historyColor;
uniform sampler2D gVelocity;
uniform sampler2D gDepth;
uniform int historyValid;
uniform int stableFrames;
uniform float currentJitterX;
uniform float currentJitterY;
uniform float previousJitterX;
uniform float previousJitterY;
uniform mat4 inverseCurrentViewProjection;
uniform mat4 previousViewProjection;
uniform vec3 currentCameraPosition;

void main()
{
    // Projection translates a surface by +jitter. History lives on the stable output grid.
    vec2 size = vec2(textureSize(currentColor, 0));
    vec2 currentJitter = vec2(currentJitterX, currentJitterY);
    vec2 previousJitter = vec2(previousJitterX, previousJitterY);
    vec2 currentUV = clamp(TexCoord + currentJitter, 0.5 / size, 1.0 - 0.5 / size);
    vec3 current = texture(currentColor, currentUV).rgb;
    ivec2 pixel = clamp(ivec2(currentUV * size), ivec2(0), ivec2(size) - 1);
    float depth = texelFetch(gDepth, pixel, 0).r;
    // Reconstruct at the depth texel's actual raster center, using the jittered matrix.
    bool background = depth >= 1.0;
    vec2 depthUV = background ? currentUV : (vec2(pixel) + 0.5) / size;
    vec4 world = inverseCurrentViewProjection * vec4(depthUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    float currentViewDepth = background ? 0.0 : 1.0 / world.w;
    world /= world.w;
    FragColor = vec4(current, currentViewDepth);
    if (historyValid == 0) return;

    // Static coverage changes are the supersamples we need to integrate, not disocclusions.
    // Use the exact stable grid, avoiding repeated history filtering from RG16F roundoff.
    if (stableFrames > 0)
    {
        vec3 stableHistory = texture(historyColor, TexCoord).rgb;
        FragColor = vec4(mix(stableHistory, current, 1.0 / float(stableFrames + 1)), currentViewDepth);
        return;
    }

    vec2 velocity = texelFetch(gVelocity, pixel, 0).rg;
    float motionPixels = length((velocity - currentJitter + previousJitter) * size);
    // velocity = (currentStable + jc) - (previousStable + jp).
    // previousStable = (outputUV + jc) - velocity - jp.
    vec2 previousUV = TexCoord + currentJitter - velocity - previousJitter;
    if (background)
    {
        // Environment directions are at infinity: reproject rotation, ignoring camera translation.
        vec4 previousDirection = previousViewProjection * vec4(world.xyz - currentCameraPosition, 0.0);
        if (previousDirection.w <= 0.0) return;
        previousUV = previousDirection.xy / previousDirection.w * 0.5 + 0.5 - previousJitter;
        motionPixels = length((TexCoord - previousUV) * size);
    }
    if (any(lessThan(previousUV, 0.5 / size)) || any(greaterThan(previousUV, 1.0 - 0.5 / size))) return;

    vec4 history = texture(historyColor, previousUV);
    float expectedPreviousDepth = background ? 0.0 : (previousViewProjection * world).w;
    // Alpha stores linear view depth. Reject newly exposed surfaces before color clipping.
    if (!background && expectedPreviousDepth <= 0.0) return;

    vec3 colorMin = vec3(1e20);
    vec3 colorMax = vec3(-1e20);
    float depthMin = expectedPreviousDepth;
    float depthMax = expectedPreviousDepth;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            ivec2 neighbor = clamp(pixel + ivec2(x, y), ivec2(0), ivec2(size) - 1);
            vec3 color = texelFetch(currentColor, neighbor, 0).rgb;
            colorMin = min(colorMin, color);
            colorMax = max(colorMax, color);
            // A single depth comparison repeatedly rejects jittered thin/alpha-tested edges.
            // Use the same surface neighborhood as color clipping, while rejecting exposed interiors.
            float neighborDepth = texelFetch(gDepth, neighbor, 0).r;
            float previousDepth = 0.0;
            if (neighborDepth < 1.0)
            {
                vec2 neighborUV = (vec2(neighbor) + 0.5) / size;
                vec4 neighborWorld = inverseCurrentViewProjection * vec4(neighborUV * 2.0 - 1.0, neighborDepth * 2.0 - 1.0, 1.0);
                neighborWorld /= neighborWorld.w;
                previousDepth = max((previousViewProjection * neighborWorld).w, 0.0);
            }
            depthMin = min(depthMin, previousDepth);
            depthMax = max(depthMax, previousDepth);
        }
    }

    float depthTolerance = max(0.02, 0.02 * expectedPreviousDepth);
    if (history.a < depthMin - depthTolerance || history.a > depthMax + depthTolerance) return;

    vec3 clampedHistory = clamp(history.rgb, colorMin, colorMax);
    float historyWeight = mix(0.9, 0.2, clamp(motionPixels / 8.0, 0.0, 1.0));
    FragColor = vec4(mix(current, clampedHistory, historyWeight), currentViewDepth);
}
