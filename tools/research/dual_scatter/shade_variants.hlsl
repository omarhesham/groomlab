// SPDX-License-Identifier: Apache-2.0
// The viewport's per-light hair shading (hosts/app/shaders/hair_lookdev.frag lobes / shadeLight),
// ported to HLSL to count what each variant of the dual scattering costs (fxc's instruction slots):
//   VARIANT 0  the shader as it is (lobes() twice when hairs are in the way)
//   VARIANT 1  exact factorization (lobes' azimuthal, Fresnel and attenuation terms once; the
//              spread costs three Gaussians)
//   VARIANT 2  azimuth-averaged closed form of the spread (one Gaussian and a fitted amplitude)
// LIGHTS: how many shadeLight calls (11 = the 3 key/fill/rim + the 8 environment lights).

#ifndef VARIANT
#define VARIANT 0
#endif
#ifndef LIGHTS
#define LIGHTS 11
#endif

cbuffer Params : register(b0) {
    float4 lightDir[LIGHTS];
    float4 lightColor[LIGHTS];  // w: count of hairs in the way
    float4 mat1;                // s, ior, shift, ms
    float4 matB;                // B, density, back scale
    float4 specR, specTT, specTRT;
    float4 fit0;                // a0 a1 a2 k1
    float4 fit1;                // k2 mu w2
};

static const float PI = 3.14159265359;
float sq(float x) { return x * x; }
float gauss(float b, float x) { return exp(-0.5 * x * x / (b * b)) / (2.50662827 * b); }
float fresnel(float ior, float c) {
    float f0 = sq((ior - 1.0) / (ior + 1.0));
    return f0 + (1.0 - f0) * pow(1.0 - saturate(c), 5.0);
}

struct Parts {
    float sinThetaH;
    float3 XR, XTT, XTRT;
};

Parts lobeParts(float3 T, float3 L, float3 V, float3 color, float B, float s, float ior) {
    Parts o;
    float sinL = clamp(dot(T, L), -1.0, 1.0), sinV = clamp(dot(T, V), -1.0, 1.0);
    float cosThetaD = max(cos(0.5 * abs(asin(sinV) - asin(sinL))), 0.05);
    float3 Lp = L - sinL * T, Vp = V - sinV * T;
    float cosPhi = dot(Lp, Vp) * rsqrt(max(dot(Lp, Lp) * dot(Vp, Vp), 1e-8));
    float cosHalfPhi = sqrt(saturate(0.5 + 0.5 * cosPhi));
    o.sinThetaH = 0.5 * (sinL + sinV);
    float Nr = 0.25 * cosHalfPhi;
    float Fr = fresnel(ior, sqrt(saturate(0.5 + 0.5 * dot(V, L))));
    o.XR = (Nr * Fr) * specR.rgb;
    float sinThetaD = sqrt(max(1.0 - cosThetaD * cosThetaD, 0.0));
    float nPrime = sqrt(max(ior * ior - sinThetaD * sinThetaD, 1.0)) / cosThetaD;
    float a = 1.0 / nPrime;
    float h = cosHalfPhi * (1.0 + a * (0.6 - 0.8 * cosPhi));
    float f = fresnel(ior, cosThetaD * sqrt(saturate(1.0 - h * h)));
    float3 Ttt = pow(color, (2.0 * sqrt(max(1.0 - h * h * a * a, 0.0)) / cosThetaD).xxx);
    float kTT = 1.0 / (1.0 / 3.65 + s * s);
    float Ntt = 0.945 * sqrt(kTT / (2.0 * PI)) * exp(kTT * (-cosPhi - 1.0));
    o.XTT = (Ntt * sq(1.0 - f)) * Ttt * specTT.rgb;
    float f2 = fresnel(ior, cosThetaD * 0.5);
    float3 Ttrt = pow(color, (3.5 / cosThetaD).xxx);
    float kTRT = 1.0 / (1.0 / 17.0 + s * s);
    float Ntrt = 0.758 * sqrt(kTRT / (2.0 * PI)) * exp(kTRT * (cosPhi - 1.0));
    o.XTRT = (Ntrt * f2 * sq(1.0 - f2)) * Ttrt * specTRT.rgb;
    return o;
}

float3 fromParts(Parts p, float B, float shift, float spreadSq) {
    float bR = sqrt(B * B + spreadSq), bTT = sqrt(0.25 * B * B + spreadSq), bTRT = sqrt(4.0 * B * B + spreadSq);
    return gauss(bR, p.sinThetaH + shift) * p.XR + gauss(bTT, p.sinThetaH - 0.5 * shift) * p.XTT +
           gauss(bTRT, p.sinThetaH - 1.5 * shift) * p.XTRT;
}

float3 shadeLight(float3 T, float3 L, float3 V, float3 color, float count, float3 af, float3 ab) {
    float B = matB.x, s = mat1.x, ior = mat1.y, shift = mat1.z, ms = mat1.w;
    float sinL = clamp(dot(T, L), -1.0, 1.0);
    float direct = exp(-count * matB.y);
    float3 Tf = (count > 0.0 ? 0.7 * pow(af, count.xxx) : 0.0.xxx) * ms;
    float bTT = 0.5 * B;
    float spreadSq = count * bTT * bTT;
    float afL = dot(af, float3(0.2126, 0.7152, 0.0722));
    float3 Ab = ab * (afL * afL);
    Parts p = lobeParts(T, L, V, color, B, s, ior);
    float sigmaB = sqrt(4.0 * B * B + bTT * bTT);
    float gb = gauss(sigmaB, p.sinThetaH - 2.0 * shift);
    float gbSpread = gauss(sqrt(sigmaB * sigmaB + spreadSq), p.sinThetaH - 2.0 * shift);
    float3 back = matB.z * 2.0 * Ab / PI * ms;
    float3 single = fromParts(p, B, shift, 0.0);
    float3 result = direct * (single + 0.7 * back * gb);
    [branch] if (count > 0.05) {
#if VARIANT == 0
        // The shader as it is: lobes() again with the widened widths.
        Parts q = lobeParts(T, L, V, color, B, s, ior);
        float3 spread = fromParts(q, B, shift, spreadSq);
#elif VARIANT == 1
        float3 spread = fromParts(p, B, shift, spreadSq);
#else
        float3 A = fit0.x + fit0.y * pow(color, fit0.www) + fit0.z * pow(color, fit1.xxx);
        float3 spread = gauss(sqrt(fit1.z * B * B + spreadSq), p.sinThetaH - fit1.y * shift) * A;
#endif
        result += Tf * (spread + PI * 0.7 * back * gbSpread);
    }
    return result * sqrt(max(1.0 - sinL * sinL, 0.0));
}

float4 main(float3 T : TEXCOORD0, float3 V : TEXCOORD1, float3 color : TEXCOORD2) : SV_Target {
    float3 af = 0.04 + 0.86 * pow(color, 1.2.xxx);
    float3 ab = 0.02 + 0.3 * pow(color, 0.85.xxx);
    float3 sum = 0.0.xxx;
    [unroll] for (int i = 0; i < LIGHTS; ++i) sum += shadeLight(T, lightDir[i].xyz, V, color, lightColor[i].w, af, ab) * lightColor[i].rgb;
    return float4(sum, 1.0);
}
