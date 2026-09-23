#pragma once
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

// oklchRampSample -node <name> [-count N | -position P] [-srgb]
// Returns a flat float array (r g b r g b ...) sampled from an oklchRamp node.
class OklchRampSampleCmd : public MPxCommand
{
public:
    MStatus doIt(const MArgList& args) override;
    bool    isUndoable() const override { return false; }

    static void*   creator() { return new OklchRampSampleCmd(); }
    static MSyntax createSyntax();

    static const char* commandName;
};
