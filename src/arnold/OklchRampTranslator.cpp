// MtoA extension: translates the Maya "oklchRamp" node into Arnold nodes.
//
//   oklchRamp.outColor  -> ramp_rgb   (N densely baked keys, linear between keys)
//   oklchRamp.outAlpha  -> MtoA routes it through the same ramp_rgb; Arnold's
//                          RGB->float conversion applies (not OKLab lightness)
//   place2dTexture on uvCoord -> uv_transform wrapping the ramp (passthrough)
//   inputMode "Input Value"    -> ramp_rgb type=custom, input <- inputValue (linkable)
//
// No custom Arnold shader is needed: the OKLCH interpolation is evaluated on
// the Maya side (shared OklchRampEval.h) and baked into the ramp keys.
#include "extension/Extension.h"
#include "translators/shader/ShaderTranslator.h"

#include <ai.h>

#include <maya/MFnDependencyNode.h>
#include <maya/MRampAttribute.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MAngle.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MColorArray.h>
#include <maya/MGlobal.h>

#include "../OklchRampEval.h"

namespace {

const int   kBakeSamples = 256;
const char* kRampTag     = "ramp";

// Returns the place2dTexture node connected to uvCoord, or a null object.
MObject connectedPlace2d(const MObject& rampNode)
{
    MFnDependencyNode fn(rampNode);
    MPlug uv = fn.findPlug("uvCoord", false);
    if (uv.isNull()) return MObject::kNullObj;
    MPlugArray srcs;
    if (!uv.connectedTo(srcs, true, false) || srcs.length() == 0) return MObject::kNullObj;
    MObject src = srcs[0].node();
    return MFnDependencyNode(src).typeName() == "place2dTexture" ? src : MObject::kNullObj;
}

} // namespace

class COklchRampTranslator : public CShaderTranslator
{
public:
    static void* creator() { return new COklchRampTranslator(); }

    AtNode* CreateArnoldNodes() override
    {
        if (!connectedPlace2d(GetMayaObject()).isNull()) {
            AtNode* xform = AddArnoldNode("uv_transform");
            AtNode* ramp  = AddArnoldNode("ramp_rgb", kRampTag);
            AiNodeLink(ramp, "passthrough", xform);
            return xform;
        }
        return AddArnoldNode("ramp_rgb");
    }

    void Export(AtNode* root) override
    {
        AtNode* ramp = GetArnoldNode(kRampTag);
        if (!ramp) ramp = root;

        exportRamp(ramp);
        if (ramp != root) exportUvTransform(root);
    }

protected:
    void NodeChanged(MObject& node, MPlug& plug) override
    {
        // Structural changes need the Arnold nodes rebuilt (uv_transform added/removed).
        const MString name = plug.partialName(false, false, false, false, false, true);
        if (name == "uvCoord" || name == "inputMode")
            SetUpdateMode(AI_RECREATE_NODE);
        CShaderTranslator::NodeChanged(node, plug);
    }

private:
    void exportRamp(AtNode* ramp)
    {
        MStatus st;
        MObject node = GetMayaObject();
        MFnDependencyNode fn(node);

        // --- read the Maya ramp + interpolation settings ---
        std::vector<oklch::RampEntry> entries;
        oklch::RampParams params;
        {
            MObject attr = fn.attribute("colorEntryList", &st);
            MRampAttribute mramp(node, attr, &st);
            if (st) {
                MIntArray indexes, interps; MFloatArray positions; MColorArray colors;
                mramp.getEntries(indexes, positions, colors, interps, &st);
                for (unsigned i = 0; i < positions.length(); ++i) {
                    oklch::RampEntry e;
                    e.position = positions[i];
                    e.color    = { colors[i].r, colors[i].g, colors[i].b };
                    e.interp   = interps[i];
                    entries.push_back(e);
                }
            }
            params.space     = static_cast<oklch::Space>  (FindMayaPlug("interpolationSpace").asShort());
            params.hue       = static_cast<oklch::HueMode>(FindMayaPlug("hueInterpolation").asShort());
            params.gamut     = static_cast<oklch::Gamut>  (FindMayaPlug("gamutMapping").asShort());
            params.srgbInput = FindMayaPlug("inputColorSpace").asShort() == 1;
        }
        oklch::PreparedRamp prepared(entries, params);

        // --- bake into dense keys ---
        AtArray* position = AiArrayAllocate(kBakeSamples, 1, AI_TYPE_FLOAT);
        AtArray* interp   = AiArrayAllocate(kBakeSamples, 1, AI_TYPE_INT);
        AtArray* color    = AiArrayAllocate(kBakeSamples, 1, AI_TYPE_RGB);
        for (int i = 0; i < kBakeSamples; ++i) {
            const double t = double(i) / double(kBakeSamples - 1);
            oklch::Vec3 c = prepared.evaluate(t);
            if (params.srgbInput)   // Arnold wants scene-linear
                c = { oklch::srgbToLinear(c.x), oklch::srgbToLinear(c.y), oklch::srgbToLinear(c.z) };

            AiArraySetFlt(position, i, float(t));
            AiArraySetInt(interp, i, 1);   // linear between the dense keys
            AiArraySetRGB(color, i, AtRGB(float(c.x), float(c.y), float(c.z)));
        }
        AiNodeSetArray(ramp, AtString("position"), position);
        AiNodeSetArray(ramp, AtString("interpolation"), interp);
        AiNodeSetArray(ramp, AtString("color"), color);
        AiNodeSetStr(ramp, AtString("wrap"), AtString("clamp"));

        // --- mapping ---
        if (FindMayaPlug("inputMode").asShort() == 1) {
            AiNodeSetStr(ramp, AtString("type"), AtString("custom"));
            ProcessParameter(ramp, "input", AI_TYPE_FLOAT, "inputValue");
        } else {
            static const char* kTypes[] = { "v", "u", "diagonal", "radial", "circular", "box" };
            int rt = FindMayaPlug("rampType").asShort();
            if (rt < 0 || rt > 5) rt = 0;
            AiNodeSetStr(ramp, AtString("type"), AtString(kTypes[rt]));
        }
    }

    void exportUvTransform(AtNode* xform)
    {
        MObject p2d = connectedPlace2d(GetMayaObject());
        if (p2d.isNull()) return;
        MFnDependencyNode fn(p2d);
        auto f  = [&](const char* a) { return fn.findPlug(a, false).asFloat(); };
        auto b  = [&](const char* a) { return fn.findPlug(a, false).asBool(); };
        auto dg = [&](const char* a) { return float(fn.findPlug(a, false).asMAngle().asDegrees()); };

        AiNodeSetVec2(xform, AtString("coverage"),        f("coverageU"),       f("coverageV"));
        AiNodeSetVec2(xform, AtString("translate_frame"), f("translateFrameU"), f("translateFrameV"));
        AiNodeSetFlt (xform, AtString("rotate_frame"),    dg("rotateFrame"));
        AiNodeSetStr (xform, AtString("unit"),            AtString("degrees"));
        AiNodeSetBool(xform, AtString("mirror_u"),        b("mirrorU"));
        AiNodeSetBool(xform, AtString("mirror_v"),        b("mirrorV"));
        AiNodeSetStr (xform, AtString("wrap_frame_u"),    AtString(b("wrapU") ? "periodic" : "color"));
        AiNodeSetStr (xform, AtString("wrap_frame_v"),    AtString(b("wrapV") ? "periodic" : "color"));
        AiNodeSetRGBA(xform, AtString("wrap_frame_color"), 0.f, 0.f, 0.f, 1.f);
        AiNodeSetBool(xform, AtString("stagger"),         b("stagger"));
        AiNodeSetVec2(xform, AtString("repeat"),          f("repeatU"),  f("repeatV"));
        AiNodeSetVec2(xform, AtString("offset"),          f("offsetU"),  f("offsetV"));
        AiNodeSetFlt (xform, AtString("rotate"),          dg("rotateUV"));
        AiNodeSetVec2(xform, AtString("noise"),           f("noiseU"),   f("noiseV"));
    }
};

// ---------------------------------------------------------------------------
// extension entry points
// ---------------------------------------------------------------------------
extern "C"
{
    DLLEXPORT void initializeExtension(CExtension& extension)
    {
        extension.Requires("oklchRamp");   // only active while the Maya plug-in is loaded
        extension.RegisterTranslator("oklchRamp", "", COklchRampTranslator::creator);
    }

    DLLEXPORT void deinitializeExtension(CExtension&) {}

    EXPORT_API_VERSION
}
