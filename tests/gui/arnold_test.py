# GUI Maya test: MtoA translator for oklchRamp. Exports .ass files and logs findings.
import os, re, traceback
import maya.cmds as cmds, maya.mel as mel

out_dir = os.environ["OKLCH_TEST_OUT"]
root = os.path.join(os.environ["OKLCH_REPO"], "module")
log = open(os.path.join(out_dir, "arnold_test.log"), "w", encoding="utf-8")
def L(*a): log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

try:
    L("MTOA_EXTENSIONS_PATH:", os.environ.get("MTOA_EXTENSIONS_PATH"))
    cmds.loadPlugin(os.path.join(root, "plug-ins", "oklchRamp.mll"))
    cmds.loadPlugin("mtoa")
    mel.eval('source "%s"' % os.path.join(root, "scripts", "oklchRamp.mel").replace("\\", "/"))
    L("mtoa loaded:", cmds.pluginInfo("mtoa", q=True, loaded=True), cmds.pluginInfo("mtoa", q=True, version=True))
    try:
        L("translators for oklchRamp:", cmds.arnoldPlugins(listTranslators="oklchRamp"))
    except Exception as e:
        L("arnoldPlugins query failed:", e)

    cmds.file(new=True, force=True)
    def plane(x, y, tex, attr="outColor", dst="color"):
        p = cmds.polyPlane(w=4, h=2, sx=1, sy=1)[0]
        cmds.move(x, y, 0, p); cmds.rotate(90, 0, 0, p)
        sh = cmds.shadingNode("lambert", asShader=True)
        sg = cmds.sets(renderable=True, noSurfaceShader=True, empty=True, name=sh + "SG")
        cmds.connectAttr(sh + ".outColor", sg + ".surfaceShader")
        cmds.connectAttr(tex + "." + attr, sh + "." + dst)
        cmds.sets(p, e=True, forceElement=sg)
        return p

    # 1: U ramp red->blue with place2dTexture (repeatU=2)
    r1 = mel.eval("oklchRampCreate()")
    mel.eval('oklchRampSetColors("%s", {1,0,0, 0,0,1})' % r1)
    cmds.setAttr(r1 + ".rampType", 1)
    p2d = cmds.listConnections(r1 + ".uvCoord", s=True, d=False)[0]
    cmds.setAttr(p2d + ".repeatU", 2)
    plane(-2.2, 1.2, r1)
    # 2: radial 3 colours, no place2d
    r2 = cmds.shadingNode("oklchRamp", asTexture=True)
    mel.eval('oklchRampSetColors("%s", {1,1,0, 0,0.5,1, 1,0,0.5})' % r2)
    cmds.setAttr(r2 + ".rampType", 3)
    plane(2.2, 1.2, r2)
    # 3: input value mode driven by a connection (place2d outU is not float; use a float constant via expression-free way)
    r3 = cmds.shadingNode("oklchRamp", asTexture=True)
    mel.eval('oklchRampSetColors("%s", {1,0,0, 0,0,1})' % r3)
    cmds.setAttr(r3 + ".inputMode", 1); cmds.setAttr(r3 + ".inputValue", 0.5)
    plane(-2.2, -1.3, r3)
    # 4: outAlpha -> ramp_float (drive lambert.color via alpha as grey? connect outAlpha to lambert.diffuse instead)
    r4 = cmds.shadingNode("oklchRamp", asTexture=True)
    mel.eval('oklchRampSetColors("%s", {0,0,0, 1,1,1})' % r4)
    cmds.setAttr(r4 + ".rampType", 1)
    p4 = plane(2.2, -1.3, r4, attr="outAlpha", dst="diffuse")

    cam, camShape = cmds.camera(name="testCam")
    cmds.setAttr(cam + ".translate", 0, 0, 12)
    cmds.setAttr("defaultRenderGlobals.currentRenderer", "arnold", type="string")
    cmds.setAttr("defaultResolution.width", 800); cmds.setAttr("defaultResolution.height", 500)


    ass = os.path.join(out_dir, "arnold_test.ass").replace("\\", "/")
    cmds.arnoldExportAss(f=ass, cam=camShape, mask=255, lightLinks=0, shadowLinks=0)
    txt = open(ass, encoding="utf-8", errors="replace").read()
    L("ass size:", len(txt))
    for n in ("ramp_rgb", "ramp_float", "uv_transform", "oklchRamp"):
        L("count of '%s' nodes/refs:" % n, len(re.findall(r"^%s\b" % n, txt, re.M)), txt.count(n))
    m = re.search(r"^ramp_rgb\n(.*?)^}", txt, re.M | re.S)
    if m:
        body = m.group(1)
        for key in ("type", "wrap", "interpolation", "input"):
            mm = re.search(r"^\s*%s\s+(.{0,60})" % key, body, re.M)
            L("ramp_rgb first:", key, "=", mm.group(1) if mm else None)
        mm = re.search(r"^\s*position\s+(\d+)\s+1\s+FLOAT", body, re.M)
        L("ramp_rgb position count:", mm.group(1) if mm else None)
    m = re.search(r"^uv_transform\n(.*?)^}", txt, re.M | re.S)
    if m:
        for key in ("passthrough", "repeat", "wrap_frame_u"):
            mm = re.search(r"^\s*%s\s+(.{0,60})" % key, m.group(1), re.M)
            L("uv_transform:", key, "=", mm.group(1) if mm else None)
    L("DONE")
except Exception:
    L("EXC", traceback.format_exc())
finally:
    log.close()
    cmds.quit(force=True)

