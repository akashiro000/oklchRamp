# Diagnostic: compare on-screen pixels of Maya's gradientControl vs the OKLCH preview strip.
# Run via  tests/run_gui_test.ps1 -Test preview_probe   (set OKLCH_SCENE to a scene path, optional)
import os, traceback, colorsys
import maya.cmds as cmds, maya.mel as mel

out_dir = os.environ["OKLCH_TEST_OUT"]
root = os.path.join(os.environ["OKLCH_REPO"], "module")
log = open(os.path.join(out_dir, "preview_probe.log"), "w", encoding="utf-8")
def L(*a): log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

def hsv(rgb):
    h, s, v = colorsys.rgb_to_hsv(*[max(0.0, min(1.0, c)) for c in rgb])
    return "H%.0f S%.2f V%.2f" % (h * 360, s, v)

def fmt(rgb): return "(%.3f %.3f %.3f) %s" % (rgb[0], rgb[1], rgb[2], hsv(rgb))

def grab(widget, fx, fy):
    img = widget.grab().toImage()
    x = int(img.width() * fx); y = int(img.height() * fy)
    c = img.pixelColor(x, y)
    return (c.redF(), c.greenF(), c.blueF()), (img.width(), img.height(), x, y)

def finish():
    L("DONE"); log.close(); cmds.quit(force=True)

def probe():
    try:
        from PySide6 import QtWidgets
        from shiboken6 import wrapInstance
        import maya.OpenMayaUI as omui

        ramp = os.environ.get("OKLCH_NODE", "oklchRamp1")
        cmds.select(ramp)
        mel.eval("openAEWindow"); mel.eval("AEbuildControls")
        cmds.refresh()

        L("cm enabled:", cmds.colorManagementPrefs(q=True, cmEnabled=True),
          "view:", cmds.colorManagementPrefs(q=True, viewTransformName=True),
          "colorPickerSpace/mixing:", cmds.colorManagementPrefs(q=True, colorPickerSpace=True) if False else "n/a")
        for i in cmds.getAttr(ramp + ".colorEntryList", mi=True):
            e = "%s.colorEntryList[%d]" % (ramp, i)
            pos = cmds.getAttr(e + ".colorEntryList_Position")
            col = cmds.getAttr(e + ".colorEntryList_Color")[0]
            disp = cmds.colorManagementConvert(toDisplaySpace=list(col))
            L("entry[%d] pos %.2f stored %s | toDisplaySpace %s" % (i, pos, fmt(col), fmt(disp)))

        # --- gradientControl of this node in the AE ---
        def utype(c):
            try: return cmds.objectTypeUI(c)
            except Exception: return ""
        grads = [c for c in (cmds.lsUI(controls=True, long=True) or []) if utype(c) == "gradientControl"]
        L("gradientControls:", len(grads))
        for g in grads:
            w = wrapInstance(int(omui.MQtUtil.findControl(g)), QtWidgets.QWidget)
            for fx in (0.03, 0.5, 0.97):
                px, info = grab(w, fx, 0.35)
                L("  gradientControl x=%.2f pixel %s  %s" % (fx, fmt(px), info))

        # --- preview strip canvases ---
        grids = [g for g in (cmds.lsUI(type="gridLayout", long=True) or []) if "oklchRampPreviewCol" in g]
        L("preview grids:", len(grids))
        if grids:
            g = grids[0]
            kids = cmds.gridLayout(g, q=True, childArray=True)
            n = len(kids)
            for idx in (0, n // 2, n - 1):
                cv = g + "|" + kids[idx]
                setv = cmds.canvas(cv, q=True, rgbValue=True)
                w = wrapInstance(int(omui.MQtUtil.findControl(cv)), QtWidgets.QWidget)
                px, info = grab(w, 0.5, 0.5)
                L("  canvas[%d] set %s | pixel %s" % (idx, fmt(setv), fmt(px)))

        # --- reference: a plain colorSliderGrp / swatch for entry[0] colour (AE colour swatch style) ---
        e0 = cmds.getAttr(ramp + ".colorEntryList", mi=True)[0]
        col0 = cmds.getAttr("%s.colorEntryList[%d].colorEntryList_Color" % (ramp, e0))[0]
        win = cmds.window(title="probe"); cmds.columnLayout()
        sw = cmds.colorSliderGrp(rgb=col0, label="managed swatch")
        cv2 = cmds.canvas(rgbValue=col0, width=60, height=20)
        cmds.showWindow(win); cmds.refresh()
        w = wrapInstance(int(omui.MQtUtil.findControl(sw)), QtWidgets.QWidget)
        px, info = grab(w, 0.42, 0.5)
        L("colorSliderGrp(entry0 raw) pixel %s %s" % (fmt(px), info))
        w = wrapInstance(int(omui.MQtUtil.findControl(cv2)), QtWidgets.QWidget)
        px, info = grab(w, 0.5, 0.5)
        L("canvas(entry0 raw) pixel %s" % fmt(px))
    except Exception:
        L("EXC", traceback.format_exc())
    finish()

try:
    cmds.loadPlugin(os.path.join(root, "plug-ins", "oklchRamp.mll"))
    for f in ("oklchRamp.mel", "AEoklchRampTemplate.mel"):
        mel.eval('source "%s"' % os.path.join(root, "scripts", f).replace("\\", "/"))
    scene = os.environ.get("OKLCH_SCENE")
    if scene:
        cmds.file(scene, open=True, force=True)   # never saved
        L("opened:", scene)
    else:
        r = mel.eval("oklchRampCreate()")
        mel.eval('oklchRampSetColors("%s", {0.5,0.25,0.5, 0.5,0.5,0.25})' % r)
        os.environ["OKLCH_NODE"] = r
    cmds.evalDeferred(probe, lowestPriority=True)
except Exception:
    L("EXC", traceback.format_exc()); finish()
