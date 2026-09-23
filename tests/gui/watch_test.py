# GUI Maya test for oklchRampWatch: edits to ramp children must trigger the command on idle.
import os, traceback
import maya.cmds as cmds, maya.mel as mel

out_dir = os.environ["OKLCH_TEST_OUT"]
root = os.path.join(os.environ["OKLCH_REPO"], "module")
log = open(os.path.join(out_dir, "watch_test.log"), "w", encoding="utf-8")
def L(*a): log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

state = {"step": 0}
steps = []

def finish():
    L("DONE"); log.close(); cmds.quit(force=True)

def run_step():
    try:
        i = state["step"]
        if i >= len(steps):
            finish(); return
        label, fn = steps[i]
        before = mel.eval("$tmp = $gOklchWatchHits")
        fn()
        def check():
            after = mel.eval("$tmp = $gOklchWatchHits")
            L("%-28s hits: %d -> %d" % (label, before, after))
            state["step"] += 1
            cmds.evalDeferred(run_step, lowestPriority=True)
        cmds.evalDeferred(check, lowestPriority=True)   # runs after the idle task
    except Exception:
        L("EXC", traceback.format_exc()); finish()

try:
    cmds.loadPlugin(os.path.join(root, "plug-ins", "oklchRamp.mll"))
    for f in ("oklchRamp.mel", "AEoklchRampTemplate.mel"):
        mel.eval('source "%s"' % os.path.join(root, "scripts", f).replace("\\", "/"))
    ramp = mel.eval("oklchRampCreate()")
    mel.eval("global int $gOklchWatchHits = 0;")
    wid = mel.eval('oklchRampWatch -node "%s" -command "global int $gOklchWatchHits; $gOklchWatchHits++;"' % ramp)
    L("watch id:", wid)

    def many_sets():
        for k in range(20):   # simulate a drag: 20 sets -> expect exactly +1
            cmds.setAttr(ramp + ".colorEntryList[1].colorEntryList_Position", 0.5 + k * 0.01)

    steps[:] = [
        ("set color[0]",      lambda: cmds.setAttr(ramp + ".colorEntryList[0].colorEntryList_Color", 1, 0, 0, type="double3")),
        ("set colorR[1]",     lambda: cmds.setAttr(ramp + ".colorEntryList[1].colorEntryList_ColorR", 0.3)),
        ("set pos[1]",        lambda: cmds.setAttr(ramp + ".colorEntryList[1].colorEntryList_Position", 0.8)),
        ("set interp[0]",     lambda: cmds.setAttr(ramp + ".colorEntryList[0].colorEntryList_Interp", 2)),
        ("new entry[2]",      lambda: cmds.setAttr(ramp + ".colorEntryList[2].colorEntryList_Position", 0.5)),
        ("remove entry[2]",   lambda: cmds.removeMultiInstance(ramp + ".colorEntryList[2]", b=True)),
        ("hueInterpolation",  lambda: cmds.setAttr(ramp + ".hueInterpolation", 1)),
        ("unrelated: rampType", lambda: cmds.setAttr(ramp + ".rampType", 2)),
        ("20 sets (drag)",    many_sets),
        ("after -remove",     lambda: (mel.eval("oklchRampWatch -remove %d" % wid),
                                       cmds.setAttr(ramp + ".colorEntryList[0].colorEntryList_ColorG", 0.5))),
        # AE: open the template for real and make sure it builds without error
        ("open AE",           lambda: (cmds.select(ramp), mel.eval("openAEWindow"), mel.eval("AEbuildControls"))),
    ]
    cmds.evalDeferred(run_step, lowestPriority=True)
except Exception:
    L("EXC", traceback.format_exc()); finish()
