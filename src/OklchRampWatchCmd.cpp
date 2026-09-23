#include "OklchRampWatchCmd.h"
#include "OklchRampNode.h"

#include <maya/MArgDatabase.h>
#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>
#include <maya/MGlobal.h>
#include <cstdint>

const char* OklchRampWatchCmd::commandName = "oklchRampWatch";
std::map<int, std::unique_ptr<OklchRampWatchCmd::Watch>> OklchRampWatchCmd::sWatches;
int OklchRampWatchCmd::sNextId = 1;

namespace {
const char* kNodeFlag    = "-n";  const char* kNodeFlagL    = "-node";
const char* kCommandFlag = "-c";  const char* kCommandFlagL = "-command";
const char* kRemoveFlag  = "-r";  const char* kRemoveFlagL  = "-remove";

// Does this plug (or any of its parents) belong to an attribute that changes
// the ramp's evaluation?
bool affectsRamp(MPlug plug)
{
    while (!plug.isNull()) {
        const MObject attr = plug.attribute();
        if (attr == OklchRampNode::aColorEntryList     ||
            attr == OklchRampNode::aInterpolationSpace ||
            attr == OklchRampNode::aHueInterpolation   ||
            attr == OklchRampNode::aGamutMapping       ||
            attr == OklchRampNode::aInputColorSpace)
            return true;
        if      (plug.isChild())   plug = plug.parent();
        else if (plug.isElement()) plug = plug.array();
        else break;
    }
    return false;
}

void idleTask(void* data)
{
    OklchRampWatchCmd::fire(static_cast<int>(reinterpret_cast<intptr_t>(data)));
}

void onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug&, void* clientData)
{
    const unsigned interesting = MNodeMessage::kAttributeSet        | MNodeMessage::kOtherPlugSet |
                                 MNodeMessage::kAttributeArrayAdded | MNodeMessage::kAttributeArrayRemoved |
                                 MNodeMessage::kConnectionMade      | MNodeMessage::kConnectionBroken;
    if (!(msg & interesting)) return;

    auto* w = static_cast<OklchRampWatchCmd::Watch*>(clientData);
    if (!w || w->pending || !affectsRamp(plug)) return;

    // Coalesce: many sets during a drag -> one MEL execution on the next idle.
    w->pending = true;
    MGlobal::executeTaskOnIdle(idleTask, reinterpret_cast<void*>(static_cast<intptr_t>(w->id)),
                               MGlobal::kLowIdlePriority);
}
} // namespace

// ---------------------------------------------------------------------------
void OklchRampWatchCmd::fire(int id)
{
    auto it = sWatches.find(id);
    if (it == sWatches.end()) return;          // removed while queued
    Watch* w = it->second.get();
    w->pending = false;
    if (!w->node.isValid() || !w->node.isAlive()) { remove(id); return; }
    MGlobal::executeCommand(w->command, false, false);
}

void OklchRampWatchCmd::remove(int id)
{
    auto it = sWatches.find(id);
    if (it == sWatches.end()) return;
    if (it->second->callback) MMessage::removeCallback(it->second->callback);
    sWatches.erase(it);
}

void OklchRampWatchCmd::removeAll()
{
    for (auto& kv : sWatches)
        if (kv.second->callback) MMessage::removeCallback(kv.second->callback);
    sWatches.clear();
}

// ---------------------------------------------------------------------------
MSyntax OklchRampWatchCmd::createSyntax()
{
    MSyntax syn;
    syn.addFlag(kNodeFlag,    kNodeFlagL,    MSyntax::kString);
    syn.addFlag(kCommandFlag, kCommandFlagL, MSyntax::kString);
    syn.addFlag(kRemoveFlag,  kRemoveFlagL,  MSyntax::kLong);
    syn.enableQuery(false);
    syn.enableEdit(false);
    return syn;
}

MStatus OklchRampWatchCmd::doIt(const MArgList& args)
{
    MStatus st;
    MArgDatabase db(syntax(), args, &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    if (db.isFlagSet(kRemoveFlag)) {
        int id = 0;
        db.getFlagArgument(kRemoveFlag, 0, id);
        remove(id);
        setResult(0);
        return MS::kSuccess;
    }

    if (!db.isFlagSet(kNodeFlag) || !db.isFlagSet(kCommandFlag)) {
        MGlobal::displayError("oklchRampWatch: use -node <name> -command <mel>, or -remove <id>.");
        return MS::kFailure;
    }
    MString nodeName, melCommand;
    db.getFlagArgument(kNodeFlag, 0, nodeName);
    db.getFlagArgument(kCommandFlag, 0, melCommand);

    MSelectionList sel;
    if (!sel.add(nodeName)) {
        MGlobal::displayError("oklchRampWatch: node not found: " + nodeName);
        return MS::kFailure;
    }
    MObject node;
    sel.getDependNode(0, node);
    if (MFnDependencyNode(node).typeId() != OklchRampNode::typeId) {
        MGlobal::displayError("oklchRampWatch: '" + nodeName + "' is not an oklchRamp node.");
        return MS::kFailure;
    }

    auto w = std::make_unique<Watch>();
    w->id      = sNextId++;
    w->node    = MObjectHandle(node);
    w->command = melCommand;
    w->callback = MNodeMessage::addAttributeChangedCallback(node, onAttributeChanged, w.get(), &st);
    CHECK_MSTATUS_AND_RETURN_IT(st);

    const int id = w->id;
    sWatches[id] = std::move(w);
    setResult(id);
    return MS::kSuccess;
}
