// Viewport 2.0 shading node override: bakes the OKLCH ramp into a 1D texture
// and feeds it to the "oklchRamp" fragment graph.
#pragma once
#include <maya/MPxShadingNodeOverride.h>
#include <maya/MObject.h>
#include <maya/MString.h>
#include <maya/MTextureManager.h>
#include <maya/MStateManager.h>
#include <vector>

class OklchRampOverride : public MHWRender::MPxShadingNodeOverride
{
public:
    static MHWRender::MPxShadingNodeOverride* creator(const MObject& obj);
    ~OklchRampOverride() override;

    MHWRender::DrawAPI supportedDrawAPIs() const override { return MHWRender::kAllDevices; }
    MString fragmentName() const override;

    void getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings) override;
    void updateDG() override;
    void updateShader(MHWRender::MShaderInstance& shader,
                      const MHWRender::MAttributeParameterMappingList& mappings) override;

    static const char* drawDbClassification;   // "drawdb/shader/texture/2d/oklchRamp"
    static const char* registrantId;
    static const int   kBakeWidth = 256;

    static MStatus registerFragments();
    static void    deregisterFragments();

private:
    explicit OklchRampOverride(const MObject& obj);
    void releaseTexture();

    MObject                          fObject;
    std::vector<float>               fPixels;      // RGBA float, kBakeWidth x 1
    int                              fRampMode = 0;
    float                            fInputValue = 0.0f;
    bool                             fTextureDirty = true;

    MHWRender::MTexture*             fTexture = nullptr;
    const MHWRender::MSamplerState*  fSampler = nullptr;

    MString fMapName, fSamplerName, fModeName, fValueName;
};
