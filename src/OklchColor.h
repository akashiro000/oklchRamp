// OKLab / OKLCH colour math (Björn Ottosson, public domain formulas).
// All RGB values here are *linear* sRGB / Rec.709 primaries unless stated.
#pragma once
#include <cmath>
#include <algorithm>

namespace oklch {

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

constexpr double kPi = 3.14159265358979323846;
constexpr double kAchromaticChroma = 1e-4;   // below this, hue is undefined

inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// ---- sRGB transfer function -------------------------------------------------
inline double srgbToLinear(double v) {
    double a = std::fabs(v);
    double r = (a <= 0.04045) ? a / 12.92 : std::pow((a + 0.055) / 1.055, 2.4);
    return v < 0.0 ? -r : r;
}
inline double linearToSrgb(double v) {
    double a = std::fabs(v);
    double r = (a <= 0.0031308) ? a * 12.92 : 1.055 * std::pow(a, 1.0 / 2.4) - 0.055;
    return v < 0.0 ? -r : r;
}

// ---- linear RGB <-> OKLab ---------------------------------------------------
inline Vec3 linearToOklab(const Vec3& c) {
    double l = 0.4122214708 * c.x + 0.5363325363 * c.y + 0.0514459929 * c.z;
    double m = 0.2119034982 * c.x + 0.6806995451 * c.y + 0.1073969566 * c.z;
    double s = 0.0883024619 * c.x + 0.2817188376 * c.y + 0.6299787005 * c.z;
    l = std::cbrt(l); m = std::cbrt(m); s = std::cbrt(s);
    return {
        0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
        1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
        0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s
    };
}

inline Vec3 oklabToLinear(const Vec3& lab) {
    double l = lab.x + 0.3963377774 * lab.y + 0.2158037573 * lab.z;
    double m = lab.x - 0.1055613458 * lab.y - 0.0638541728 * lab.z;
    double s = lab.x - 0.0894841775 * lab.y - 1.2914855480 * lab.z;
    l = l * l * l; m = m * m * m; s = s * s * s;
    return {
         4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s
    };
}

// ---- OKLab <-> OKLCH (hue in degrees, [0, 360)) ------------------------------
inline Vec3 oklabToOklch(const Vec3& lab) {
    double c = std::hypot(lab.y, lab.z);
    double h = 0.0;
    if (c > kAchromaticChroma) {
        h = std::atan2(lab.z, lab.y) * 180.0 / kPi;
        if (h < 0.0) h += 360.0;
    }
    return { lab.x, c, h };
}
inline Vec3 oklchToOklab(const Vec3& lch) {
    double rad = lch.z * kPi / 180.0;
    return { lch.x, lch.y * std::cos(rad), lch.y * std::sin(rad) };
}

// ---- gamut handling ---------------------------------------------------------
inline bool inGamut(const Vec3& rgb, double eps = 1e-4) {
    return rgb.x >= -eps && rgb.x <= 1.0 + eps &&
           rgb.y >= -eps && rgb.y <= 1.0 + eps &&
           rgb.z >= -eps && rgb.z <= 1.0 + eps;
}
inline Vec3 clipRgb(const Vec3& rgb) {
    return { clamp01(rgb.x), clamp01(rgb.y), clamp01(rgb.z) };
}

// Reduce chroma (keeping L and hue) until the colour fits in the display gamut.
inline Vec3 reduceChromaToGamut(const Vec3& lch) {
    if (lch.x <= 0.0) return { 0.0, 0.0, 0.0 };
    if (lch.x >= 1.0) return { 1.0, 1.0, 1.0 };
    Vec3 rgb = oklabToLinear(oklchToOklab(lch));
    if (inGamut(rgb)) return clipRgb(rgb);
    double lo = 0.0, hi = lch.y;
    for (int i = 0; i < 24; ++i) {
        double mid = 0.5 * (lo + hi);
        Vec3 test = oklabToLinear(oklchToOklab({ lch.x, mid, lch.z }));
        if (inGamut(test)) lo = mid; else hi = mid;
    }
    return clipRgb(oklabToLinear(oklchToOklab({ lch.x, lo, lch.z })));
}

} // namespace oklch
