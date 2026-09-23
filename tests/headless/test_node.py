# Headless regression test. Run:  mayapy tests/headless/test_node.py
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MODULE = os.path.join(ROOT, "module")
import maya.standalone
maya.standalone.initialize(name="python")
import maya.cmds as cmds
import maya.mel as mel

root = MODULE
cmds.loadPlugin(os.path.join(root, "plug-ins", "oklchRamp.mll"))
print("loaded:", cmds.pluginInfo("oklchRamp", q=True, loaded=True))

# source MEL files (syntax check + procs)
for f in ("oklchRamp.mel", "AEoklchRampTemplate.mel"):
    mel.eval('source "%s"' % os.path.join(root, "scripts", f).replace("\\", "/"))
print("procs:", mel.eval("exists oklchRampCreate"), mel.eval("exists AEoklchRampTemplate"))

# create with helper -> place2dTexture wiring
ramp = mel.eval("oklchRampCreate()")
print("node:", ramp, cmds.nodeType(ramp), "classification:", cmds.getClassification("oklchRamp"))
print("uv connected:", cmds.listConnections(ramp + ".uvCoord", s=True, d=False))

# default entries
print("default entries:", cmds.getAttr(ramp + ".colorEntryList", mi=True))

# red -> blue ramp
mel.eval('oklchRampSetColors("%s", {1,0,0, 0,0,1})' % ramp)
cmds.setAttr(ramp + ".inputMode", 1)

def sample(t):
    cmds.setAttr(ramp + ".inputValue", t)
    return cmds.getAttr(ramp + ".outColor")[0], cmds.getAttr(ramp + ".outAlpha")

for space, label in ((0, "OKLCH"), (1, "OKLab")):
    cmds.setAttr(ramp + ".interpolationSpace", space)
    print(label, "t=0.0", sample(0.0))
    print(label, "t=0.5", sample(0.5))
    print(label, "t=1.0", sample(1.0))

cmds.setAttr(ramp + ".interpolationSpace", 0)
for hue, label in ((0, "shorter"), (1, "longer"), (2, "increasing"), (3, "decreasing")):
    cmds.setAttr(ramp + ".hueInterpolation", hue)
    print("hue", label, "t=0.5", sample(0.5)[0])

# gamut mapping modes
for g in (0, 1, 2):
    cmds.setAttr(ramp + ".gamutMapping", g)
    print("gamut", g, sample(0.5)[0])

# sample command
print("cmd -p 0.5:", cmds.oklchRampSample(node=ramp, position=0.5))
print("cmd -c 5 -srgb:", cmds.oklchRampSample(node=ramp, count=5, srgb=True))

# uv mode: U ramp via uvCoord
cmds.setAttr(ramp + ".inputMode", 0)
cmds.setAttr(ramp + ".rampType", 1)
cmds.disconnectAttr(cmds.listConnections(ramp + ".uvCoord", s=True, d=False, p=True)[0], ramp + ".uvCoord")
cmds.setAttr(ramp + ".uvCoord", 0.25, 0.9, type="float2")
print("uv U ramp u=0.25:", cmds.getAttr(ramp + ".outColor")[0])

# save / reload round trip
cmds.setAttr(ramp + ".hueInterpolation", 1)
path = os.path.join(os.environ["TEMP"], "oklch_test.ma")
cmds.file(rename=path); cmds.file(save=True, type="mayaAscii", force=True)
cmds.file(new=True, force=True)
cmds.file(path, open=True, force=True)
print("reloaded entries:", cmds.getAttr(ramp + ".colorEntryList", mi=True),
      "hue:", cmds.getAttr(ramp + ".hueInterpolation"),
      "color[1]:", cmds.getAttr(ramp + ".colorEntryList[1].colorEntryList_Color"))
print("ALL OK")
