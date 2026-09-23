# Headless test for AEoklchRamp_previewAddEntry (click-to-add). Run:  mayapy tests/headless/test_add_entry.py
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MODULE = os.path.join(ROOT, "module")
import maya.standalone; maya.standalone.initialize(name="python")
import maya.cmds as cmds, maya.mel as mel

root = MODULE
cmds.loadPlugin(os.path.join(root, "plug-ins", "oklchRamp.mll"))
for f in ("oklchRamp.mel", "AEoklchRampTemplate.mel"):
    mel.eval('source "%s"' % os.path.join(root, "scripts", f).replace("\\", "/"))

ramp = mel.eval("oklchRampCreate()")
mel.eval('oklchRampSetColors("%s", {1,0,0, 0,0,1})' % ramp)
cmds.setAttr(ramp + ".colorEntryList[0].colorEntryList_Interp", 2)  # smooth, should be inherited
cmds.setAttr(ramp + ".inputMode", 1)
cmds.setAttr(ramp + ".inputValue", 0.5)
before = cmds.getAttr(ramp + ".outColor")[0]
before25 = cmds.oklchRampSample(node=ramp, position=0.25)

cmds.undoInfo(state=True, infinity=True)
mel.eval('AEoklchRamp_previewAddEntry("%s", 32, 65)' % ramp)   # pos = 0.5
idx = cmds.getAttr(ramp + ".colorEntryList", mi=True)
print("indices:", idx)
e = ramp + ".colorEntryList[%d]" % idx[-1]
print("new pos:", cmds.getAttr(e + ".colorEntryList_Position"),
      "color:", cmds.getAttr(e + ".colorEntryList_Color"),
      "interp:", cmds.getAttr(e + ".colorEntryList_Interp"))
after = cmds.getAttr(ramp + ".outColor")[0]
after25 = cmds.oklchRampSample(node=ramp, position=0.25)
print("shape preserved at 0.5:", all(abs(a - b) < 1e-5 for a, b in zip(before, after)), before, after)
print("shape preserved at 0.25:", all(abs(a - b) < 1e-3 for a, b in zip(before25, after25)))
cmds.undo()
print("after undo indices:", cmds.getAttr(ramp + ".colorEntryList", mi=True))
print("ALL OK")
