#include "OklchRampNode.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MRampAttribute.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MColorArray.h>
#include <maya/MPlug.h>
#include <maya/MGlobal.h>
#include <cmath>
#include <algorithm>

const MString OklchRampNode::typeName("oklchRamp");
// Registered Autodesk node ID block: 0x00142C40 - 0x00142C7F (64 IDs).
// 0x00142C40 = oklchRamp. Reserve subsequent IDs for future nodes of this plug-in.
const MTypeId OklchRampNode::typeId(0x00142C40);
const MString OklchRampNode::classification("texture/2d:drawdb/shader/texture/2d/oklchRamp");

MObject OklchRampNode::aColorEntryList;
MObject OklchRampNode::aRampType;
MObject OklchRampNode::aInputMode;
MObject OklchRampNode::aInputValue;
MObject OklchRampNode::aInterpolationSpace;
MObject OklchRampNode::aHueInterpolation;
MObject OklchRampNode::aGamutMapping;
MObject OklchRampNode::aInputColorSpace;
MObject OklchRampNode::aUVCoord;
MObject OklchRampNode::aUVFilterSize;
MObject OklchRampNode::aOutColor;
MObject OklchRampNode::aOutAlpha;

#define MAKE_INPUT(attr)                    \
    CHECK_MSTATUS(attr.setKeyable(true));   \
    CHECK_MSTATUS(attr.setStorable(true));  \
    CHECK_MSTATUS(attr.setReadable(true));  \
    CHECK_MSTATUS(attr.setWritable(true));

#define MAKE_OUTPUT(attr)                   \
    CHECK_MSTATUS(attr.setKeyable(false));  \
    CHECK_MSTATUS(attr.setStorable(false)); \
    CHECK_MSTATUS(attr.setReadable(true));  \
    CHECK_MSTATUS(attr.setWritable(false));

void* OklchRampNode::creator() { return new OklchRampNode(); }

MStatus OklchRampNode::initialize()
{
    MStatus st;
    MFnNumericAttribute nAttr;
    MFnEnumAttribute    eAttr;

    // --- ramp (same structure as Maya's ramp so AEaddRampControl works) ---
    aColorEntryList = MRampAttribute::createColorRamp("colorEntryList", "cel", &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    CHECK_MSTATUS(addAttribute(aColorEntryList));

    // --- ramp shape ---
    aRampType = eAttr.create("rampType", "rt", 0, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    eAttr.addField("V Ramp",    0);
    eAttr.addField("U Ramp",    1);
    eAttr.addField("Diagonal",  2);
    eAttr.addField("Radial",    3);
    eAttr.addField("Circular",  4);
    eAttr.addField("Box",       5);
    MAKE_INPUT(eAttr);
    CHECK_MSTATUS(addAttribute(aRampType));

    aInputMode = eAttr.create("inputMode", "im", 0, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    eAttr.addField("UV Coordinates", 0);
    eAttr.addField("Input Value",    1);
    MAKE_INPUT(eAttr);
    CHECK_MSTATUS(addAttribute(aInputMode));

    aInputValue = nAttr.create("inputValue", "iv", MFnNumericData::kFloat, 0.0f, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    nAttr.setSoftMin(0.0f); nAttr.setSoftMax(1.0f);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS(addAttribute(aInputValue));

    // --- interpolation ---
    aInterpolationSpace = eAttr.create("interpolationSpace", "isp", 0, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    eAttr.addField("OKLCH", 0);
    eAttr.addField("OKLab", 1);
    MAKE_INPUT(eAttr);
    CHECK_MSTATUS(addAttribute(aInterpolationSpace));

    aHueInterpolation = eAttr.create("hueInterpolation", "hi", 0, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    eAttr.addField("Shorter",    0);
    eAttr.addField("Longer",     1);
    eAttr.addField("Increasing", 2);
    eAttr.addField("Decreasing", 3);
    MAKE_INPUT(eAttr);
    CHECK_MSTATUS(addAttribute(aHueInterpolation));

    aGamutMapping = eAttr.create("gamutMapping", "gm", 2, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    eAttr.addField("None",          0);
    eAttr.addField("Clip",          1);
    eAttr.addField("Reduce Chroma", 2);
    MAKE_INPUT(eAttr);
    CHECK_MSTATUS(addAttribute(aGamutMapping));

    aInputColorSpace = eAttr.create("inputColorSpace", "ics", 0, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    eAttr.addField("Linear (scene-linear)", 0);
    eAttr.addField("sRGB (display)",        1);
    MAKE_INPUT(eAttr);
    CHECK_MSTATUS(addAttribute(aInputColorSpace));

    // --- uv (hidden, driven by place2dTexture) ---
    MObject uCoord = nAttr.create("uCoord", "u", MFnNumericData::kFloat, 0.0f);
    MObject vCoord = nAttr.create("vCoord", "v", MFnNumericData::kFloat, 0.0f);
    aUVCoord = nAttr.create("uvCoord", "uv", uCoord, vCoord, MObject::kNullObj, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS(nAttr.setHidden(true));
    CHECK_MSTATUS(addAttribute(aUVCoord));

    MObject fsx = nAttr.create("uvFilterSizeX", "fsx", MFnNumericData::kFloat, 0.0f);
    MObject fsy = nAttr.create("uvFilterSizeY", "fsy", MFnNumericData::kFloat, 0.0f);
    aUVFilterSize = nAttr.create("uvFilterSize", "fs", fsx, fsy, MObject::kNullObj, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS(nAttr.setHidden(true));
    CHECK_MSTATUS(addAttribute(aUVFilterSize));

    // --- outputs ---
    aOutColor = nAttr.createColor("outColor", "oc", &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    MAKE_OUTPUT(nAttr);
    CHECK_MSTATUS(addAttribute(aOutColor));

    aOutAlpha = nAttr.create("outAlpha", "oa", MFnNumericData::kFloat, 0.0f, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    MAKE_OUTPUT(nAttr);
    CHECK_MSTATUS(addAttribute(aOutAlpha));

    // --- dependencies ---
    const MObject inputs[] = {
        aColorEntryList, aRampType, aInputMode, aInputValue, aInterpolationSpace,
        aHueInterpolation, aGamutMapping, aInputColorSpace, aUVCoord, aUVFilterSize
    };
    for (const MObject& in : inputs) {
        CHECK_MSTATUS(attributeAffects(in, aOutColor));
        CHECK_MSTATUS(attributeAffects(in, aOutAlpha));
    }
    return MS::kSuccess;
}

void OklchRampNode::postConstructor()
{
    // Default ramp: black -> white, like Maya's ramp texture.
    MStatus st;
    MRampAttribute ramp(thisMObject(), aColorEntryList, &st);
    if (!st) return;

    // Maya auto-creates entry [0] on a fresh ramp attribute; reuse it instead of
    // adding a duplicate at position 0.
    if (ramp.getNumEntries() > 0) {
        ramp.setPositionAtIndex(0.0f, 0);
        MColor black(0.0f, 0.0f, 0.0f);
        ramp.setColorAtIndex(black, 0);
        ramp.setInterpolationAtIndex(MRampAttribute::kLinear, 0);
    } else {
        MFloatArray p; p.append(0.0f);
        MColorArray c; c.append(MColor(0.0f, 0.0f, 0.0f));
        MIntArray   i; i.append(MRampAttribute::kLinear);
        ramp.addEntries(p, c, i);
    }
    MFloatArray positions;  positions.append(1.0f);
    MColorArray colors;     colors.append(MColor(1.0f, 1.0f, 1.0f));
    MIntArray   interps;    interps.append(MRampAttribute::kLinear);
    ramp.addEntries(positions, colors, interps);
}

MStatus OklchRampNode::readRamp(const MObject& node,
                                std::vector<oklch::RampEntry>& entries,
                                oklch::RampParams& params)
{
    MStatus st;
    MFnDependencyNode fn(node, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    MRampAttribute ramp(node, aColorEntryList, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    MIntArray indexes, interps;
    MFloatArray positions;
    MColorArray colors;
    ramp.getEntries(indexes, positions, colors, interps, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    entries.clear();
    entries.reserve(positions.length());
    for (unsigned i = 0; i < positions.length(); ++i) {
        oklch::RampEntry e;
        e.position = positions[i];
        e.color    = { colors[i].r, colors[i].g, colors[i].b };
        e.interp   = interps[i];
        entries.push_back(e);
    }

    params.space     = static_cast<oklch::Space>  (fn.findPlug(aInterpolationSpace, false).asShort());
    params.hue       = static_cast<oklch::HueMode>(fn.findPlug(aHueInterpolation,   false).asShort());
    params.gamut     = static_cast<oklch::Gamut>  (fn.findPlug(aGamutMapping,       false).asShort());
    params.srgbInput = fn.findPlug(aInputColorSpace, false).asShort() == 1;
    return MS::kSuccess;
}

MStatus OklchRampNode::compute(const MPlug& plug, MDataBlock& block)
{
    if (plug != aOutColor && plug.parent() != aOutColor && plug != aOutAlpha)
        return MS::kUnknownParameter;

    // --- ramp position ---
    double t = 0.0;
    if (block.inputValue(aInputMode).asShort() == 1) {
        t = block.inputValue(aInputValue).asFloat();
    } else {
        const float2& uv = block.inputValue(aUVCoord).asFloat2();
        const double u = uv[0], v = uv[1];
        const double du = u - 0.5, dv = v - 0.5;
        switch (block.inputValue(aRampType).asShort()) {
        case 1:  t = u; break;                                  // U ramp
        case 2:  t = 0.5 * (u + v); break;                      // Diagonal
        case 3: {                                               // Radial
            double a = std::atan2(dv, du) / (2.0 * oklch::kPi);
            t = a < 0.0 ? a + 1.0 : a;
            break;
        }
        case 4:  t = 2.0 * std::sqrt(du * du + dv * dv); break; // Circular
        case 5:  t = 2.0 * std::max(std::fabs(du), std::fabs(dv)); break; // Box
        case 0:
        default: t = v; break;                                  // V ramp
        }
    }

    // --- evaluate ---
    std::vector<oklch::RampEntry> entries;
    oklch::RampParams params;
    MStatus st = readRamp(thisMObject(), entries, params);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    oklch::PreparedRamp ramp(entries, params);
    oklch::Vec3 rgb = ramp.evaluate(t);

    MDataHandle outColor = block.outputValue(aOutColor);
    outColor.asFloatVector() = MFloatVector(float(rgb.x), float(rgb.y), float(rgb.z));
    outColor.setClean();

    MDataHandle outAlpha = block.outputValue(aOutAlpha);
    outAlpha.asFloat() = float(oklch::PreparedRamp::lightness(rgb, params.srgbInput));
    outAlpha.setClean();

    return MS::kSuccess;
}
