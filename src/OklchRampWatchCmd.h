// oklchRampWatch -node <name> -command <mel>   -> int watch id
// oklchRampWatch -remove <id>
//
// Runs <mel> (once per idle, coalesced) whenever the ramp entries or any
// interpolation setting of an oklchRamp node change. Unlike
// `scriptJob -attributeChange`, this also fires for edits to children of the
// colorEntryList compound array (what the gradient editor does while dragging).
#pragma once
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MMessage.h>
#include <maya/MObjectHandle.h>
#include <maya/MString.h>
#include <map>
#include <memory>

class OklchRampWatchCmd : public MPxCommand
{
public:
    MStatus doIt(const MArgList& args) override;
    bool    isUndoable() const override { return false; }

    static void*   creator() { return new OklchRampWatchCmd(); }
    static MSyntax createSyntax();

    static const char* commandName;

    struct Watch {
        int           id = 0;
        MObjectHandle node;
        MString       command;
        MCallbackId   callback = 0;
        bool          pending = false;   // idle execution already queued
    };

    static void remove(int id);
    static void removeAll();             // plug-in unload
    static void fire(int id);            // idle task: run the MEL command

private:
    static std::map<int, std::unique_ptr<Watch>> sWatches;
    static int sNextId;
};
