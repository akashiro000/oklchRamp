// Ramp evaluation shared by the node's compute() and the sampling command.
#pragma once
#include "OklchColor.h"
#include <vector>
#include <algorithm>

namespace oklch {

enum class Space    { OKLCH = 0, OKLab = 1 };
enum class HueMode  { Shorter = 0, Longer = 1, Increasing = 2, Decreasing = 3 };
enum class Gamut    { None = 0, Clip = 1, ReduceChroma = 2 };
enum class Interp   { None = 0, Linear = 1, Smooth = 2, Spline = 3 }; // matches MRampAttribute

struct RampEntry {
    double position = 0.0;
    Vec3   color;          // RGB as stored on the node
    int    interp = 1;
};

struct RampParams {
    Space   space     = Space::OKLCH;
    HueMode hue       = HueMode::Shorter;
    Gamut   gamut     = Gamut::ReduceChroma;
    bool    srgbInput = false;   // true: entries are display sRGB, apply transfer in/out
};

namespace detail {

inline double hueDelta(double h0, double h1, HueMode mode) {
    double d = std::fmod(h1 - h0, 360.0);
    if (d < 0.0) d += 360.0;                 // d in [0, 360)
    switch (mode) {
    case HueMode::Increasing: return d;
    case HueMode::Decreasing: return d == 0.0 ? 0.0 : d - 360.0;
    case HueMode::Longer:
        if (d == 0.0) return 0.0;
        return d < 180.0 ? d - 360.0 : d;
    case HueMode::Shorter:
    default:
        return d >= 180.0 ? d - 360.0 : d;
    }
}

inline double catmullRom(double p0, double p1, double p2, double p3, double t) {
    double t2 = t * t, t3 = t2 * t;
    return 0.5 * ((2.0 * p1) + (-p0 + p2) * t +
                  (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                  (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}

inline Vec3 mix(const Vec3& a, const Vec3& b, double s) {
    return { a.x + (b.x - a.x) * s, a.y + (b.y - a.y) * s, a.z + (b.z - a.z) * s };
}

} // namespace detail

// Prepared (converted + sorted) ramp for repeated sampling.
class PreparedRamp {
public:
    PreparedRamp(const std::vector<RampEntry>& entries, const RampParams& params)
        : m_params(params)
    {
        m_entries = entries;
        std::stable_sort(m_entries.begin(), m_entries.end(),
            [](const RampEntry& a, const RampEntry& b) { return a.position < b.position; });

        const size_t n = m_entries.size();
        m_comp.resize(n);
        for (size_t i = 0; i < n; ++i) {
            Vec3 rgb = m_entries[i].color;
            if (params.srgbInput)
                rgb = { srgbToLinear(rgb.x), srgbToLinear(rgb.y), srgbToLinear(rgb.z) };
            Vec3 lab = linearToOklab(rgb);
            m_comp[i] = (params.space == Space::OKLCH) ? oklabToOklch(lab) : lab;
        }

        if (params.space == Space::OKLCH && n > 0) {
            // 1) achromatic entries borrow the hue of their nearest chromatic neighbour
            std::vector<bool> chromatic(n);
            bool anyChromatic = false;
            for (size_t i = 0; i < n; ++i) {
                chromatic[i] = m_comp[i].y > kAchromaticChroma;
                anyChromatic = anyChromatic || chromatic[i];
            }
            if (anyChromatic) {
                for (size_t i = 1; i < n; ++i)
                    if (!chromatic[i] && chromatic[i - 1]) { m_comp[i].z = m_comp[i - 1].z; chromatic[i] = true; }
                for (size_t i = n - 1; i-- > 0;)
                    if (!chromatic[i] && chromatic[i + 1]) { m_comp[i].z = m_comp[i + 1].z; chromatic[i] = true; }
            }
            // 2) unwrap hue along the ramp according to the hue mode
            for (size_t i = 1; i < n; ++i)
                m_comp[i].z = m_comp[i - 1].z + detail::hueDelta(m_comp[i - 1].z, m_comp[i].z, params.hue);
        }
    }

    bool empty() const { return m_entries.empty(); }

    // Returns RGB in the same space the entries were given in.
    Vec3 evaluate(double t) const {
        const size_t n = m_entries.size();
        if (n == 0) return { 0.0, 0.0, 0.0 };
        if (n == 1 || t <= m_entries.front().position) return finish(m_comp.front());
        if (t >= m_entries.back().position)             return finish(m_comp.back());

        size_t i = 0;
        while (i + 1 < n && m_entries[i + 1].position <= t) ++i;
        if (i + 1 >= n) return finish(m_comp.back());

        const double p0 = m_entries[i].position, p1 = m_entries[i + 1].position;
        const double span = p1 - p0;
        if (span <= 1e-9) return finish(m_comp[i + 1]);
        const double u = (t - p0) / span;

        const Interp interp = static_cast<Interp>(m_entries[i].interp);
        Vec3 c;
        switch (interp) {
        case Interp::None:
            c = m_comp[i];
            break;
        case Interp::Smooth:
            c = detail::mix(m_comp[i], m_comp[i + 1], u * u * (3.0 - 2.0 * u));
            break;
        case Interp::Spline: {
            const Vec3& a  = m_comp[i > 0 ? i - 1 : i];
            const Vec3& b  = m_comp[i];
            const Vec3& cc = m_comp[i + 1];
            const Vec3& d  = m_comp[i + 2 < n ? i + 2 : i + 1];
            c = { detail::catmullRom(a.x, b.x, cc.x, d.x, u),
                  detail::catmullRom(a.y, b.y, cc.y, d.y, u),
                  detail::catmullRom(a.z, b.z, cc.z, d.z, u) };
            break;
        }
        case Interp::Linear:
        default:
            c = detail::mix(m_comp[i], m_comp[i + 1], u);
            break;
        }
        return finish(c);
    }

    // Lightness (OKLab L) of a colour, handy for outAlpha.
    static double lightness(const Vec3& rgbStored, bool srgbInput) {
        Vec3 rgb = rgbStored;
        if (srgbInput) rgb = { srgbToLinear(rgb.x), srgbToLinear(rgb.y), srgbToLinear(rgb.z) };
        return linearToOklab(rgb).x;
    }

private:
    Vec3 finish(const Vec3& comp) const {
        Vec3 lch = (m_params.space == Space::OKLCH) ? comp : oklabToOklch(comp);
        if (lch.y < 0.0) lch.y = 0.0;   // spline overshoot can make chroma negative

        Vec3 rgb;
        switch (m_params.gamut) {
        case Gamut::ReduceChroma: rgb = reduceChromaToGamut(lch); break;
        case Gamut::Clip:         rgb = clipRgb(oklabToLinear(oklchToOklab(lch))); break;
        case Gamut::None:
        default:                  rgb = oklabToLinear(oklchToOklab(lch)); break;
        }
        if (m_params.srgbInput)
            rgb = { linearToSrgb(rgb.x), linearToSrgb(rgb.y), linearToSrgb(rgb.z) };
        return rgb;
    }

    std::vector<RampEntry> m_entries;
    std::vector<Vec3>      m_comp;     // per-entry (L,C,H unwrapped) or (L,a,b)
    RampParams             m_params;
};

} // namespace oklch
