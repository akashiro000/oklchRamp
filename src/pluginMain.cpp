#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include "OklchRampNode.h"
#include "OklchRampSampleCmd.h"
#include "OklchRampOverride.h"
#include "OklchRampWatchCmd.h"
#include <maya/MDrawRegistry.h>

MStatus initializePlugin(MObject obj)
{
    MFnPlugin plugin(obj, "oklchRamp", "1.0.0", "Any");

    MStatus st = plugin.registerNode(OklchRampNode::typeName,
                                     OklchRampNode::typeId,
                                     OklchRampNode::creator,
                                     OklchRampNode::initialize,
                                     MPxNode::kDependNode,
                                     &OklchRampNode::classification);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    st = plugin.registerCommand(OklchRampSampleCmd::commandName,
                                OklchRampSampleCmd::creator,
                                OklchRampSampleCmd::createSyntax);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    st = plugin.registerCommand(OklchRampWatchCmd::commandName,
                                OklchRampWatchCmd::creator,
                                OklchRampWatchCmd::createSyntax);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    // Viewport 2.0 (no-op when there is no renderer, e.g. mayapy / batch)
    st = OklchRampOverride::registerFragments();
    CHECK_MSTATUS_AND_RETURN_IT(st);
    st = MHWRender::MDrawRegistry::registerShadingNodeOverrideCreator(
        OklchRampOverride::drawDbClassification,
        OklchRampOverride::registrantId,
        OklchRampOverride::creator);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    // Source the helper script (oklchRampCreate) if it is on MAYA_SCRIPT_PATH.
    MGlobal::executeCommandOnIdle(
        "if (!`exists oklchRampCreate`) { catchQuiet(eval(\"source oklchRamp.mel\")); }");
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
    MFnPlugin plugin(obj);
    MStatus st = MHWRender::MDrawRegistry::deregisterShadingNodeOverrideCreator(
        OklchRampOverride::drawDbClassification, OklchRampOverride::registrantId);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    OklchRampOverride::deregisterFragments();

    OklchRampWatchCmd::removeAll();
    st = plugin.deregisterCommand(OklchRampWatchCmd::commandName);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    st = plugin.deregisterCommand(OklchRampSampleCmd::commandName);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    st = plugin.deregisterNode(OklchRampNode::typeId);
    CHECK_MSTATUS_AND_RETURN_IT(st);
    return MS::kSuccess;
}
