#include "OklchRampSampleCmd.h"
#include "OklchRampNode.h"

#include <maya/MArgDatabase.h>
#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MDoubleArray.h>
#include <maya/MGlobal.h>

const char* OklchRampSampleCmd::commandName = "oklchRampSample";

namespace {
const char* kNodeFlag     = "-n";  const char* kNodeFlagL     = "-node";
const char* kCountFlag    = "-c";  const char* kCountFlagL    = "-count";
const char* kPositionFlag = "-p";  const char* kPositionFlagL = "-position";
const char* kSrgbFlag     = "-s";  const char* kSrgbFlagL     = "-srgb";
}

MSyntax OklchRampSampleCmd::createSyntax()
{
    MSyntax syn;
    syn.addFlag(kNodeFlag,     kNodeFlagL,     MSyntax::kString);
    syn.addFlag(kCountFlag,    kCountFlagL,    MSyntax::kLong);
    syn.addFlag(kPositionFlag, kPositionFlagL, MSyntax::kDouble);
    syn.addFlag(kSrgbFlag,     kSrgbFlagL);
    syn.enableQuery(false);
    syn.enableEdit(false);
    return syn;
}

MStatus OklchRampSampleCmd::doIt(const MArgList& args)
{
    MStatus st;
    MArgDatabase db(syntax(), args, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    if (!db.isFlagSet(kNodeFlag)) {
        MGlobal::displayError("oklchRampSample: -node flag is required.");
        return MS::kFailure;
    }
    MString nodeName;
    db.getFlagArgument(kNodeFlag, 0, nodeName);

    MSelectionList sel;
    if (!sel.add(nodeName)) {
        MGlobal::displayError("oklchRampSample: node not found: " + nodeName);
        return MS::kFailure;
    }
    MObject node;
    sel.getDependNode(0, node);
    MFnDependencyNode fn(node);
    if (fn.typeId() != OklchRampNode::typeId) {
        MGlobal::displayError("oklchRampSample: '" + nodeName + "' is not an oklchRamp node.");
        return MS::kFailure;
    }

    std::vector<oklch::RampEntry> entries;
    oklch::RampParams params;
    st = OklchRampNode::readRamp(node, entries, params);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    oklch::PreparedRamp ramp(entries, params);

    // -srgb: convert linear output to display sRGB (no-op if entries are already sRGB)
    const bool toSrgb = db.isFlagSet(kSrgbFlag) && !params.srgbInput;

    MDoubleArray result;
    auto push = [&](const oklch::Vec3& c) {
        oklch::Vec3 o = c;
        if (toSrgb)
            o = { oklch::linearToSrgb(o.x), oklch::linearToSrgb(o.y), oklch::linearToSrgb(o.z) };
        result.append(o.x); result.append(o.y); result.append(o.z);
    };

    if (db.isFlagSet(kPositionFlag)) {
        double p = 0.0;
        db.getFlagArgument(kPositionFlag, 0, p);
        push(ramp.evaluate(p));
    } else {
        int count = 32;
        if (db.isFlagSet(kCountFlag)) db.getFlagArgument(kCountFlag, 0, count);
        if (count < 1) count = 1;
        for (int i = 0; i < count; ++i) {
            double p = (count == 1) ? 0.0 : double(i) / double(count - 1);
            push(ramp.evaluate(p));
        }
    }

    setResult(result);
    return MS::kSuccess;
}
