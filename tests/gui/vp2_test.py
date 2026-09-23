# Run via  tests/run_gui_test.ps1 -Test vp2  (needs a GUI Maya; not runnable in mayapy).
import os, traceback
import maya.cmds as cmds
import maya.mel as mel

out_dir = os.environ["OKLCH_TEST_OUT"]
root = os.path.join(os.environ["OKLCH_REPO"], "module")
log_path = os.path.join(out_dir, "vp2_test.log")
log = open(log_path, "w", encoding="utf-8")

def L(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

try:
    cmds.loadPlugin(os.path.join(root, "plug-ins", "oklchRamp.mll"))
    mel.eval('source "%s"' % os.path.join(root, "scripts", "oklchRamp.mel").replace("\\", "/"))
    L("plugin loaded:", cmds.pluginInfo("oklchRamp", q=True, loaded=True))
    L("renderer:", cmds.ogs(q=True, deviceInformation=True)[:2])

    import maya.api.OpenMayaRender as omr
    fm = omr.MRenderer.getFragmentManager()
    for n in ("oklchRampBase", "oklchRampRGB", "oklchRampA", "oklchRamp"):
        L("fragment", n, "registered:", fm.hasFragment(n))

    # scene: two planes. left = OKLCH ramp (U), right = Maya ramp for comparison
    cmds.file(new=True, force=True)
    def make_plane(x, tex):
        p = cmds.polyPlane(w=4, h=2, sx=1, sy=1)[0]
        cmds.move(x, 0, 0, p)
        cmds.rotate(90, 0, 0, p)
        sh = cmds.shadingNode("lambert", asShader=True)
        sg = cmds.sets(renderable=True, noSurfaceShader=True, empty=True, name=sh + "SG")
        cmds.connectAttr(sh + ".outColor", sg + ".surfaceShader")
        cmds.connectAttr(tex + ".outColor", sh + ".color")
        cmds.sets(p, e=True, forceElement=sg)
        return p

    ramp = mel.eval("oklchRampCreate()")
    mel.eval('oklchRampSetColors("%s", {1,0,0, 0,0,1})' % ramp)
    cmds.setAttr(ramp + ".rampType", 1)  # U ramp
    make_plane(-2.2, ramp)

    mramp = cmds.shadingNode("ramp", asTexture=True)
    p2d = cmds.shadingNode("place2dTexture", asUtility=True)
    cmds.connectAttr(p2d + ".outUV", mramp + ".uvCoord")
    cmds.setAttr(mramp + ".type", 1)
    cmds.setAttr(mramp + ".colorEntryList[0].position", 0); cmds.setAttr(mramp + ".colorEntryList[0].color", 1, 0, 0, type="double3")
    cmds.setAttr(mramp + ".colorEntryList[1].position", 1); cmds.setAttr(mramp + ".colorEntryList[1].color", 0, 0, 1, type="double3")
    make_plane(2.2, mramp)

    # second row: radial + inputValue mode
    ramp2 = mel.eval("oklchRampCreate()")
    mel.eval('oklchRampSetColors("%s", {1,1,0, 0,0.5,1, 1,0,0.5})' % ramp2)
    cmds.setAttr(ramp2 + ".rampType", 3)
    pr = make_plane(-2.2, ramp2); cmds.move(-2.2, -2.5, 0, pr)
    ramp3 = mel.eval("oklchRampCreate()")
    mel.eval('oklchRampSetColors("%s", {1,0,0, 0,0,1})' % ramp3)
    cmds.setAttr(ramp3 + ".inputMode", 1); cmds.setAttr(ramp3 + ".inputValue", 0.5)
    pv = make_plane(2.2, ramp3); cmds.move(2.2, -2.5, 0, pv)

    cam = cmds.camera(name="testCam")[0]
    cmds.setAttr(cam + ".translate", 0, -1.2, 12)
    cmds.lookThru(cam)
    cmds.setAttr("hardwareRenderingGlobals.multiSampleEnable", 0)

    img = os.path.join(out_dir, "vp2_test")
    cmds.setAttr("defaultRenderGlobals.imageFormat", 32)  # png
    res = cmds.ogsRender(camera=cam, width=800, height=500, currentFrame=True, noRenderView=True)
    L("ogsRender result:", res)
    # ogsRender writes into <project>/images/tmp/<scene>.png; copy the newest file out
    import shutil, glob
    tmp = os.path.join(cmds.workspace(q=True, rd=True), "images", "tmp")
    imgs = sorted(glob.glob(os.path.join(tmp, "*.png")), key=os.path.getmtime)
    L("images:", imgs[-3:])
    if imgs:
        shutil.copy(imgs[-1], os.path.join(out_dir, "vp2_test.png"))
        L("copied", imgs[-1])

    # sample the plane's material via the override path is not scriptable; also dump shader compile errors
    L("last error:", mel.eval("getLastError"))
    L("DONE")
except Exception:
    L("EXC", traceback.format_exc())
finally:
    log.close()
    cmds.quit(force=True)
