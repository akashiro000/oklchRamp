# Regression: a ramp whose entry [0] was deleted must not regain a black entry on scene load.
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MODULE = os.path.join(ROOT, "module")
import maya.standalone; maya.standalone.initialize(name="python")
import maya.cmds as cmds

cmds.loadPlugin(os.path.join(MODULE, "plug-ins", "oklchRamp.mll"))

def entries(node):
    out = []
    for i in cmds.getAttr(node + ".colorEntryList", mi=True) or []:
        e = "%s.colorEntryList[%d]" % (node, i)
        out.append((i, round(cmds.getAttr(e + ".colorEntryList_Position"), 3),
                    tuple(round(v, 3) for v in cmds.getAttr(e + ".colorEntryList_Color")[0])))
    return out

ok = True
for label, prep in (
    ("delete [0], keep [1],[2]", lambda n: (cmds.removeMultiInstance(n + ".colorEntryList[0]", b=True),
                                             cmds.setAttr(n + ".colorEntryList[1].colorEntryList_Position", 0.0),
                                             cmds.setAttr(n + ".colorEntryList[1].colorEntryList_Color", 0.5, 0, 1, type="double3"),
                                             cmds.setAttr(n + ".colorEntryList[2].colorEntryList_Position", 1.0),
                                             cmds.setAttr(n + ".colorEntryList[2].colorEntryList_Color", 1, 1, 0, type="double3"))),
    ("only [5] and [7]",        lambda n: (cmds.removeMultiInstance(n + ".colorEntryList[0]", b=True),
                                             cmds.removeMultiInstance(n + ".colorEntryList[1]", b=True),
                                             cmds.setAttr(n + ".colorEntryList[5].colorEntryList_Position", 0.0),
                                             cmds.setAttr(n + ".colorEntryList[5].colorEntryList_Color", 0.5, 0, 1, type="double3"),
                                             cmds.setAttr(n + ".colorEntryList[7].colorEntryList_Position", 1.0),
                                             cmds.setAttr(n + ".colorEntryList[7].colorEntryList_Color", 1, 1, 0, type="double3"))),
    ("defaults untouched",      lambda n: None),
):
    cmds.file(new=True, force=True)
    n = cmds.shadingNode("oklchRamp", asTexture=True)
    prep(n)
    before = entries(n)
    path = os.path.join(os.environ["TEMP"], "oklch_reload_test.ma")
    cmds.file(rename=path); cmds.file(save=True, type="mayaAscii", force=True)
    cmds.file(new=True, force=True)
    cmds.file(path, open=True, force=True)
    after = entries(n)
    same = before == after
    ok = ok and same
    print("%-28s before %s\n%-28s after  %s  -> %s" % (label, before, "", after, "OK" if same else "MISMATCH"))

# a brand-new node (not from file) must still get the black->white default
cmds.file(new=True, force=True)
n = cmds.shadingNode("oklchRamp", asTexture=True)
d = entries(n)
print("new node defaults:", d)
ok = ok and d == [(0, 0.0, (0.0, 0.0, 0.0)), (1, 1.0, (1.0, 1.0, 1.0))]
print("ALL OK" if ok else "FAILED")
