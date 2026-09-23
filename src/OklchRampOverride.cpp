#include "OklchRampOverride.h"
#include "OklchRampNode.h"
#include "OklchRampFragments.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MGlobal.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MFragmentManager.h>
#include <maya/MShaderManager.h>
#include <maya/MTextureManager.h>

const char* OklchRampOverride::drawDbClassification = "drawdb/shader/texture/2d/oklchRamp";
const char* OklchRampOverride::registrantId         = "oklchRampPlugin";

// ---------------------------------------------------------------------------
// fragment registration
// ---------------------------------------------------------------------------
MStatus OklchRampOverride::registerFragments()
{
    MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
    if (!renderer) return MS::kSuccess;              // batch / mayapy: no VP2
    MHWRender::MFragmentManager* fm = renderer->getFragmentManager();
    if (!fm) return MS::kSuccess;

    struct Frag { const char* name; const char* xml; bool graph; };
    const Frag frags[] = {
        { oklchFragments::kBaseName,  oklchFragments::kBaseXML,  false },
        { oklchFragments::kRGBName,   oklchFragments::kRGBXML,   false },
        { oklchFragments::kAName,     oklchFragments::kAXML,     false },
        { oklchFragments::kGraphName, oklchFragments::kGraphXML, true  },
    };
    for (const Frag& f : frags) {
        if (fm->hasFragment(f.name)) continue;
        MString added = f.graph ? fm->addFragmentGraphFromBuffer(f.xml)
                                : fm->addShadeFragmentFromBuffer(f.xml, false);
        if (added != f.name) {
            MGlobal::displayError(MString("oklchRamp: failed to register VP2 fragment '") + f.name + "'");
            return MS::kFailure;
        }
    }
    return MS::kSuccess;
}

void OklchRampOverride::deregisterFragments()
{
    MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
    if (!renderer) return;
    MHWRender::MFragmentManager* fm = renderer->getFragmentManager();
    if (!fm) return;
    const char* names[] = { oklchFragments::kGraphName, oklchFragments::kAName,
                            oklchFragments::kRGBName,   oklchFragments::kBaseName };
    for (const char* n : names)
        if (fm->hasFragment(n)) fm->removeFragment(n);
}

// ---------------------------------------------------------------------------
// override
// ---------------------------------------------------------------------------
MHWRender::MPxShadingNodeOverride* OklchRampOverride::creator(const MObject& obj)
{
    return new OklchRampOverride(obj);
}

OklchRampOverride::OklchRampOverride(const MObject& obj)
    : MHWRender::MPxShadingNodeOverride(obj), fObject(obj)
{
    fPixels.assign(kBakeWidth * 4, 0.0f);
}

OklchRampOverride::~OklchRampOverride()
{
    releaseTexture();
    if (fSampler) {
        MHWRender::MStateManager::releaseSamplerState(fSampler);
        fSampler = nullptr;
    }
}

void OklchRampOverride::releaseTexture()
{
    if (!fTexture) return;
    if (MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer())
        if (MHWRender::MTextureManager* tm = renderer->getTextureManager())
            tm->releaseTexture(fTexture);
    fTexture = nullptr;
}

MString OklchRampOverride::fragmentName() const
{
    return oklchFragments::kGraphName;
}

void OklchRampOverride::getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings)
{
    // These parameters have no matching attribute; we set them in updateShader().
    // uvCoord is auto-mapped by name (place2dTexture.outUV -> uvCoord).
    const char* manual[] = { "map", "mapSampler", "rampMode", "inputValue" };
    for (const char* p : manual) {
        MHWRender::MAttributeParameterMapping m(p, "", false, true);
        mappings.append(m);
    }
}

void OklchRampOverride::updateDG()
{
    if (fObject.isNull()) return;
    MFnDependencyNode fn(fObject);

    const int inputMode = fn.findPlug(OklchRampNode::aInputMode, false).asShort();
    fRampMode   = (inputMode == 1) ? 6 : fn.findPlug(OklchRampNode::aRampType, false).asShort();
    fInputValue = fn.findPlug(OklchRampNode::aInputValue, false).asFloat();

    std::vector<oklch::RampEntry> entries;
    oklch::RampParams params;
    if (!OklchRampNode::readRamp(fObject, entries, params)) return;
    oklch::PreparedRamp ramp(entries, params);

    std::vector<float> pixels(kBakeWidth * 4);
    for (int i = 0; i < kBakeWidth; ++i) {
        const double t = double(i) / double(kBakeWidth - 1);
        const oklch::Vec3 c = ramp.evaluate(t);
        pixels[i * 4 + 0] = float(c.x);
        pixels[i * 4 + 1] = float(c.y);
        pixels[i * 4 + 2] = float(c.z);
        pixels[i * 4 + 3] = float(oklch::PreparedRamp::lightness(c, params.srgbInput));
    }
    if (pixels != fPixels) {
        fPixels.swap(pixels);
        fTextureDirty = true;
    }
}

void OklchRampOverride::updateShader(MHWRender::MShaderInstance& shader,
                                     const MHWRender::MAttributeParameterMappingList& mappings)
{
    // Resolve the (possibly renamed) parameter names once.
    if (fMapName.length() == 0) {
        auto resolve = [&](const char* name, MString& out) {
            const MHWRender::MAttributeParameterMapping* m = mappings.findByParameterName(name);
            if (m) out = m->resolvedParameterName();
        };
        resolve("map",        fMapName);
        resolve("mapSampler", fSamplerName);
        resolve("rampMode",   fModeName);
        resolve("inputValue", fValueName);
    }
    if (fMapName.length() == 0) return;

    if (fModeName.length())  shader.setParameter(fModeName,  fRampMode);
    if (fValueName.length()) shader.setParameter(fValueName, fInputValue);

    MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
    if (!renderer) return;

    // sampler: linear, clamped so t in [0,1] never wraps
    if (!fSampler) {
        MHWRender::MSamplerStateDesc sd;
        sd.filter   = MHWRender::MSamplerState::kMinMagMipLinear;
        sd.addressU = MHWRender::MSamplerState::kTexClamp;
        sd.addressV = MHWRender::MSamplerState::kTexClamp;
        sd.addressW = MHWRender::MSamplerState::kTexClamp;
        fSampler = MHWRender::MStateManager::acquireSamplerState(sd);
    }
    if (fSampler && fSamplerName.length())
        shader.setParameter(fSamplerName, *fSampler);

    // texture: rebuild when the baked ramp changed
    if (fTextureDirty || !fTexture) {
        releaseTexture();
        if (MHWRender::MTextureManager* tm = renderer->getTextureManager()) {
            MHWRender::MTextureDescription desc;
            desc.setToDefault2DTexture();
            desc.fWidth         = kBakeWidth;
            desc.fHeight        = 1;
            desc.fDepth         = 1;
            desc.fFormat        = MHWRender::kR32G32B32A32_FLOAT;
            desc.fBytesPerRow   = kBakeWidth * 4 * sizeof(float);
            desc.fBytesPerSlice = desc.fBytesPerRow;
            desc.fMipmaps       = 1;
            desc.fArraySlices   = 1;
            desc.fTextureType   = MHWRender::kImage2D;
            // empty name: never shared / cached by the manager
            fTexture = tm->acquireTexture("", desc, fPixels.data(), false);
        }
        fTextureDirty = false;
    }
    if (fTexture) {
        MHWRender::MTextureAssignment ta;
        ta.texture = fTexture;
        shader.setParameter(fMapName, ta);
    }
}
