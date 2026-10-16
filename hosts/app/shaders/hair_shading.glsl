// SPDX-License-Identifier: Apache-2.0
// The hair's shading, shared by the per-fragment and per-point shaders (hair_lookdev.frag) and the
// visibility buffer's deferred shading (hair_shade.comp): the Frame block, the lights and the
// environment (a few directional lights plus spherical harmonics) around the lobes, the fiber's
// color and the dual scattering of engine/gpu/shaders/hair_bsdf.glsl, the one source Maya's
// viewport shades with too. The number of hairs in the way of each light comes from the caller
// (the deep opacity maps or the voxel volumes: the hairs a ray crosses).

layout(std140, binding = 0) uniform Frame {
    mat4 domViewProj[5];  // the deep opacity maps' views: the three lights, the sky, the camera
    vec4 domAxis[5];      // xyz: each view's forward axis; w: the offset distances count from
    vec4 domParams;    // x: layer thickness, y: shadow density, z: depth bias, w: the camera map's layer thickness
    vec4 domParams2;   // x: unused, y: flip y, z: ambient occlusion strength, w: texel size
    vec4 eye;          // xyz; w: frame index
    vec4 mat0;         // melanin, redness, randomize, pigment source (0 groom melanin, 1 groom rgb, 2 material)
    vec4 dye;          // rgb the base color (the dye); w: longitudinal width B
    vec4 mat1;         // azimuthal spread, ior, shift (radians), multiple scattering
    vec4 specR;        // rgb weight x tint; w: diffuse weight
    vec4 specTRT;      // rgb; w: flags (1 stochastic coverage, 2 shadows, 4 ambient occlusion)
    vec4 specTT;       // rgb; w: opacity
    vec4 lightDir[3];  // xyz toward the light; w: 1 when it casts map 0
    vec4 lightColor[3];
    vec4 sh[9];
    vec4 envDir[8];    // xyz; w: active
    vec4 envColor[8];
    vec4 iridescence;  // rgb: the structural color; w: its hue's turn as the view goes from facing the strand to edge-on
    vec4 env;
    vec4 bg;
    vec4 tune;
    vec4 shadowParams; // x: a surface shadow map's texel in world units, y: filter radius (texels), z: map size, w: 1 when the maps hold the surfaces
    mat4 shadowViewProj[3];
    vec4 shadowAxis[3];
    vec4 debug;
    // The voxel volumes (the visibility buffer's shadows, lookdev_renderer.cpp's kVolume*): the
    // grid's lowest corner (w: 1 when the volumes hold the hair), one over its extent per axis
    // (uvw = (p - lo) x scale; w: a voxel's size), x: the hairs a unit of density counts for,
    // yzw: toward the sky (the ambient occlusion's volume).
    vec4 voxelLo;
    vec4 voxelScale;
    vec4 voxelParams;
    vec4 tune2;  // GROOMLAB_APP_TUNE's numbers 5 to 8 (hairCalibration2)
    vec4 tune3;  // ... 9 to 12 (hairCalibration3)
    vec4 tune4;  // ... 13 to 16: this file's (the environment's irradiance and its occlusion)
    vec4 wet;    // wet hair, soaked (core's wetShadingUniform: hair_bsdf.glsl hairWet)
    vec4 rgbLook;  // x: the azimuthal roughness the groom's RGB is absorbed by (hair_bsdf.glsl hairColorSigma)
};

// The surfaces' shadow maps (the head's shadow on the hair; the hair's own is the maps' or the
// volumes').
layout(binding = 25) uniform sampler2D shadowMap0;
layout(binding = 26) uniform sampler2D shadowMap1;
layout(binding = 27) uniform sampler2D shadowMap2;

const float PI = 3.14159265359;

// The shared lobes' calibration overridden (GROOMLAB_APP_TUNE, the Arnold comparison's tuning loop:
// 0 keeps hair_bsdf.glsl's own).
vec4 hairCalibration() { return tune; }
vec4 hairCalibration2() { return tune2; }
vec4 hairCalibration3() { return tune3; }

// (hair_bsdf.glsl comes before this file, in the shaders that include both: qsb's dependency file
// lists only the shader's own includes.)

// An integer hash (Jarzynski and Olano 2020's pcg3d: line_coverage.glsl's linePcg3d, included before
// this file as hair_bsdf.glsl is) for the stochastic coverage and the shadow taps: white over pixels
// and frames, so the accumulation averages it out.
vec3 random3(uvec3 seed) { return vec3(linePcg3d(seed)) * (1.0 / 4294967296.0); }

// How much of light `which` the surfaces let through to `pos` (0 shadowed, 1 lit): four
// taps of the surfaces' shadow map on a small disk rotated per pixel and frame (the
// accumulation makes it a soft disk), with a bias of a few texels: a strand is thin and
// hangs off the surfaces, a slope-scaled bias is not needed.
float surfaceShadow(int which, vec3 pos, float rotation) {
    if (shadowParams.w < 0.5) return 1.0;
    vec4 lp = shadowViewProj[which] * vec4(pos, 1.0);
    vec2 uv = lp.xy / lp.w * 0.5 + 0.5;
    if (domParams2.y > 0.5) uv.y = 1.0 - uv.y;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 1.0;
    float receiver = dot(pos, shadowAxis[which].xyz) - shadowAxis[which].w - shadowParams.x * 2.5;
    float lit = 0.0;
    float c = cos(rotation), s = sin(rotation);
    for (int k = 0; k < 4; ++k) {
        float r = sqrt((float(k) + 0.5) / 4.0) * shadowParams.y;
        float a = float(k) * 2.39996323;
        vec2 o = vec2(cos(a), sin(a)) * r;
        o = vec2(c * o.x - s * o.y, s * o.x + c * o.y) / shadowParams.z;
        float d = which == 0 ? textureLod(shadowMap0, uv + o, 0.0).r : which == 1 ? textureLod(shadowMap1, uv + o, 0.0).r
                                                                     : textureLod(shadowMap2, uv + o, 0.0).r;
        lit += (d <= 0.0 || receiver <= d) ? 1.0 : 0.0;
    }
    return lit * 0.25;
}

// The R lobe's tint from the structural color (white without it): hairShade sets it per point.
vec3 specularTint = vec3(1.0);

// A point of a strand to shade: where (P), along (T, unit), toward the viewer (V, unit), across the
// ribbon (side, -1..1: the cylinder's normal there), its pigment (melanin, redness, the point's
// iridescence share, its wetness), its own RGB, the scalp's normal at its root and the strand's random number.
struct HairPoint {
    vec3 P;
    vec3 T;
    vec3 V;
    float side;
    vec4 pigment;
    vec3 rgb;
    vec3 rootNormal;
    float random;
};

// The fiber's color: the pigment's absorption (eumelanin and pheomelanin at 6.6 x melanin^2 as
// aiStandardHair renders them: hair_bsdf.glsl's hairMelaninSigma, core's hairMelaninAbsorption)
// times the dye, or the groom's RGB channels as Arnold's base color over no melanin (absorbed as
// Arnold absorbs it, hairColorSigma: the hair looks the color picked); the structural color mixed
// in (setting the R lobe's tint, specularTint).
vec3 fiberColor(HairPoint h) {
    float pigment = mat0.w;
    // Structural color (FeatherColor's iridescence, the point's share of it): the fiber's color and its
    // highlight's tint go toward it, its hue turned as the view leaves the strand's normal (the
    // root's, leaned along the strand as the viewport's strand frames lean it: a feather's vane),
    // face-on its own, edge-on turned by the whole shift.
    float iridescent = clamp(h.pigment.z, 0.0, 1.0);
    vec3 tint = vec3(1.0);
    specularTint = vec3(1.0);
    if (iridescent > 0.0) {
        vec3 up = normalize(h.rootNormal);
        vec3 leaned = up - h.T * (0.5 * dot(up, h.T));
        vec3 normal = dot(leaned, leaned) > 1e-8 ? normalize(leaned) : up;
        tint = hairHue(iridescence.rgb, iridescence.w * (1.0 - abs(dot(h.V, normal))));
        specularTint = mix(vec3(1.0), tint, iridescent);
    }
    vec3 color;
    if (pigment > 0.5 && pigment < 1.5) {
        // The RGB with the structural color mixed in (Arnold's base color: the iridescence
        // network's mix), absorbed as aiStandardHair absorbs its base color.
        color = hairFiberColor(hairColorSigma(mix(h.rgb, tint, iridescent), rgbLook.x), vec3(1.0));
    } else {
        float melanin = pigment < 0.5 ? h.pigment.x : clamp(mat0.x + mat0.z * (h.random * 2.0 - 1.0), 0.0, 1.0);
        float redness = pigment < 0.5 ? h.pigment.y : mat0.y;
        color = hairFiberColor(hairMelaninSigma(melanin, redness), dye.rgb);
        if (iridescent > 0.0) color = mix(color, hairFiberColor(hairColorSigma(tint, rgbLook.x), vec3(1.0)), iridescent);
    }
    return clamp(color, vec3(1e-4), vec3(1.0));
}

// The point lit: the three lights (each through `lightCount[i]` hairs, and the surfaces' shadow
// map where `lightMapped[i]`, else what the head hides from it by the root's normal), the
// environment's lights (through the hairs above: `skyCount`, the ambient occlusion's) and its
// irradiance, Arnold's optional diffuse lobe. `rotation` turns the surface shadow taps' disk.
vec3 hairShade(HairPoint h, float lightCount[3], bool lightMapped[3], float skyCount, float rotation) {
    vec3 T = h.T, V = h.V;
    // The strand as a cylinder: its normal at this fragment (side across the ribbon) leans from
    // the view-facing normal toward the ribbon's edge; the diffuse lobe and the environment's
    // irradiance read it. (A strand seen end on has no facing direction of its own: any normal
    // across it will do, rather than the normalized zero vector's NaN.)
    vec3 facing = V - T * dot(V, T);
    float facingLength = length(facing);
    facing = facingLength > 1e-4 ? facing / facingLength : normalize(cross(T, abs(T.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0)));
    vec3 across = cross(T, facing);
    float side = clamp(h.side, -1.0, 1.0);
    vec3 N = normalize(facing * sqrt(1.0 - side * side) + across * side);
    vec3 color = fiberColor(h);

    HairLobes m;
    m.shape = vec4(dye.w, mat1.x, mat1.z, mat1.y);
    m.weightR = specR.rgb * specularTint;
    m.weightTT = specTT.rgb;
    m.weightTRT = specTRT.rgb;
    m.multiple = mat1.w;
    // Wet hair (the point's wetness, Wet's channel): darker, more saturated, a sharper highlight.
    hairWet(h.pigment.w, wet, color, m);
    vec3 af, ab;
    hairAttenuations(color, af, ab);

    // What the head hides: light from below the scalp's tangent plane at the strand's root
    // does not reach it (the lights with a map see the scalp in it; the others, and the
    // environment's, go by this).
    vec3 rootNormal = normalize(h.rootNormal);
    vec3 result = vec3(0.0);
    for (int i = 0; i < 3; ++i) {
        if (lightColor[i].w < 0.5) continue;
        vec3 L = lightDir[i].xyz;
        float visible = lightMapped[i] ? surfaceShadow(i, h.P, rotation) : smoothstep(-0.3, 0.2, dot(rootNormal, L));
        result += hairShadeLight(T, L, V, color, m, hairShadowCount(lightCount[i] * domParams.y), af, ab) * lightColor[i].rgb * visible;
    }
    // The environment: its lights through the lobes, occluded by the head and by the hair
    // above (the sky's count), and its irradiance as the light scattered in from everywhere, by
    // the fiber's albedo.
    float aoCount = skyCount * domParams2.z;
    for (int i = 0; i < 8; ++i) {
        if (envDir[i].w < 0.5) continue;
        vec3 L = envDir[i].xyz;
        float count = aoCount * (0.5 + 0.5 * L.y);
        float visible = smoothstep(-0.3, 0.2, dot(rootNormal, L));
        result += hairShadeLight(T, L, V, color, m, hairShadowCount(count * domParams.y), af, ab) * envColor[i].rgb * (env.x * visible);
    }
    vec3 irr = sh[0].rgb + sh[1].rgb * N.y + sh[2].rgb * N.z + sh[3].rgb * N.x + sh[4].rgb * (N.x * N.y) +
               sh[5].rgb * (N.y * N.z) + sh[6].rgb * (N.z * N.z) + sh[7].rgb * (N.x * N.z) + sh[8].rgb * (N.x * N.x - N.y * N.y);
    irr = max(irr, vec3(0.0)) * env.x;
    vec3 albedo = hairAmbientAlbedo(color, m.multiple);
    float aoTerm = exp(-aoCount * hairCal(tune4.y, 0.29));
    result += irr * albedo * (hairCal(tune4.x, 0.063) / PI) * aoTerm;
    // Arnold's optional diffuse lobe (Kajiya-Kay's), for grooms that use it.
    if (specR.w > 0.0) {
        vec3 diff = vec3(0.0);
        for (int i = 0; i < 3; ++i) {
            if (lightColor[i].w < 0.5) continue;
            float tl = dot(T, lightDir[i].xyz);
            float shadow = exp(-hairShadowCount(lightCount[i] * domParams.y));
            diff += lightColor[i].rgb * sqrt(max(0.0, 1.0 - tl * tl)) * shadow;
        }
        result += specR.w * color * diff / PI;
    }
    return result;
}
