#pragma once
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>
#include <maya/MString.h>
#include <vector>
#include "OklchRampEval.h"

class OklchRampNode : public MPxNode
{
public:
    OklchRampNode() = default;
    ~OklchRampNode() override = default;

    void    postConstructor() override;
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return SchedulingType::kParallel; }

    static void*   creator();
    static MStatus initialize();

    // Read ramp entries + interpolation params from any oklchRamp node.
    static MStatus readRamp(const MObject& node,
                            std::vector<oklch::RampEntry>& entries,
                            oklch::RampParams& params);

    static const MString  typeName;
    static const MTypeId  typeId;
    static const MString  classification;

    // attributes
    static MObject aColorEntryList;
    static MObject aRampType;
    static MObject aInputMode;
    static MObject aInputValue;
    static MObject aInterpolationSpace;
    static MObject aHueInterpolation;
    static MObject aGamutMapping;
    static MObject aInputColorSpace;
    static MObject aUVCoord;
    static MObject aUVFilterSize;
    static MObject aOutColor;
    static MObject aOutAlpha;
};
