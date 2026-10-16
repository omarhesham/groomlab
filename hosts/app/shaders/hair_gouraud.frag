#version 440
// SPDX-License-Identifier: Apache-2.0

// The interactive frame's ribbons, lit per point: the shaded color came with the vertex
// (hair.vert under HAIR_GOURAUD, from the per-point shading pass), interpolated along the
// ribbon; this fragment does the coverage alone. The strands draw twice as hair_lookdev.frag
// has them: those a pixel wide or more opaque with their depth, then the thinner ones blended
// over everything, each weighed by the transmittance of the hair in front of it from the
// camera's deep opacity map. Linear HDR out, premultiplied by the coverage.

// Every output of hair.vert is declared, read or not: Direct3D 11 links the stages by the
// semantics in order, and a gap before vShade left it reading another varying.
layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vTangent;
layout(location = 2) in vec4 vAttr;     // side, u, coverage, random
layout(location = 3) in vec4 vPigment;
layout(location = 4) in vec3 vColor;
layout(location = 5) in vec3 vRootNormal;
layout(location = 6) in vec3 vShade;    // the point's shaded color

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Frame {
    mat4 domViewProj[5];  // the deep opacity maps' views: the three lights, the sky, the camera
    vec4 domAxis[5];      // xyz: each view's forward axis; w: the offset distances count from
    vec4 domParams;    // x: layer thickness, y: shadow density, z: depth bias, w: the camera map's layer thickness
    vec4 domParams2;   // x: unused, y: flip y, z: ambient occlusion strength, w: texel size
    vec4 eye;
    vec4 mat0;
    vec4 dye;
    vec4 mat1;
    vec4 specR;
    vec4 specTRT;
    vec4 specTT;       // w: opacity
    vec4 lightDir[3];
    vec4 lightColor[3];
    vec4 sh[9];
    vec4 envDir[8];
    vec4 envColor[8];
    vec4 scalp;
    vec4 env;
    vec4 bg;
    vec4 tune;
    vec4 shadowParams;
    mat4 shadowViewProj[3];
    vec4 shadowAxis[3];
    vec4 debug;
};

layout(std140, binding = 1) uniform ViewBlock {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 view;
    vec4 pixel;  // w: the pass: 5 the strands at least a pixel wide, 6 the thinner ones
    vec4 axis;
    vec4 dom;
};

// The camera's deep opacity map (lookdev_renderer.cpp's kDomCamera).
layout(binding = 12) uniform sampler2D domDepth4;
layout(binding = 13) uniform sampler2D domLayers4;
const int kDomCamera = 4;
const float kPassThick = 5.0;

// The number of hairs between the camera and `pos` (coverage-weighted), as
// hair_lookdev.frag's hairCount reads the camera's map.
float cameraHairCount(vec3 pos) {
    vec4 lp = domViewProj[kDomCamera] * vec4(pos, 1.0);
    vec2 uv = lp.xy / lp.w * 0.5 + 0.5;
    if (domParams2.y > 0.5) uv.y = 1.0 - uv.y;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    float front = texture(domDepth4, uv).r;
    float t = domParams.w;
    float d = dot(pos, domAxis[kDomCamera].xyz) - domAxis[kDomCamera].w - front;
    if (d <= 0.0) return 0.0;
    vec4 layers = texture(domLayers4, uv);
    if (d <= t) return mix(0.0, layers.r, d / t);
    if (d <= 2.0 * t) return mix(layers.r, layers.g, (d - t) / t);
    if (d <= 4.0 * t) return mix(layers.g, layers.b, (d - 2.0 * t) / (2.0 * t));
    if (d <= 8.0 * t) return mix(layers.b, layers.a, (d - 4.0 * t) / (4.0 * t));
    return layers.a;
}

void main() {
    float coverage = vAttr.z * specTT.w;
    bool thick = coverage >= 0.999;
    if ((pixel.w < kPassThick + 0.5) != thick) discard;
    if (!thick) {
        float through = exp(-cameraHairCount(vPosition));
        if (through < 0.05) discard;  // buried: little of it would show
        coverage *= through;
    }
    fragColor = vec4(vShade * coverage, coverage);
}
