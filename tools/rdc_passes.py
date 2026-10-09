# banjo-nx: dumps the colour target of every render pass of a RenderDoc capture (run inside qrenderdoc):
#   qrenderdoc.exe --python tools/rdc_passes.py
# Reads the capture path and output folder from RDC_IN / RDC_OUT (environment). For each run of consecutive draws
# into the same colour target it saves that target after the run's last draw (PNG) and writes passes.txt with
# event ids, draw counts, target size and format.
import os
import sys
import traceback

import renderdoc as rd

IN = os.environ.get("RDC_IN")
OUT = os.environ.get("RDC_OUT")
log = open(os.path.join(OUT, "passes.txt"), "w")


def say(*parts):
    log.write(" ".join(str(p) for p in parts) + "\n")
    log.flush()


def flatten(actions, out):
    for a in actions:
        out.append(a)
        flatten(a.children, out)
    return out


try:
    cap = rd.OpenCaptureFile()
    status = cap.OpenFile(IN, "", None)
    if status != rd.ResultCode.Succeeded:
        raise RuntimeError("open file: %s" % status)
    result = cap.OpenCapture(rd.ReplayOptions(), None)
    status, controller = result if isinstance(result, tuple) else (rd.ResultCode.Succeeded, result)
    if status != rd.ResultCode.Succeeded:
        raise RuntimeError("open capture: %s" % status)
    sfile = controller.GetStructuredFile()
    textures = {t.resourceId: t for t in controller.GetTextures()}
    actions = flatten(controller.GetRootActions(), [])
    draws = [a for a in actions if a.flags & rd.ActionFlags.Drawcall]
    say("actions", len(actions), "draws", len(draws))

    passes = []  # [target, first eid, last eid, count, last name]
    for a in draws:
        target = a.outputs[0] if len(a.outputs) and a.outputs[0] != rd.ResourceId.Null() else a.depthOut
        if passes and passes[-1][0] == target:
            passes[-1][2] = a.eventId
            passes[-1][3] += 1
        else:
            passes.append([target, a.eventId, a.eventId, 1])
    say("passes", len(passes))
    for i, (target, first, last, count) in enumerate(passes):
        t = textures.get(target)
        desc = "%dx%d %s msaa%d" % (t.width, t.height, t.format.Name(), t.msSamp) if t else "?"
        name = "pass_%03d_e%d.png" % (i, last)
        saved = False
        if t and t.width >= 64 and t.height >= 32:
            controller.SetFrameEvent(last, True)
            save = rd.TextureSave()
            save.resourceId = target
            save.destType = rd.FileType.PNG
            save.alpha = rd.AlphaMapping.Discard
            save.mip = 0
            save.slice.sliceIndex = 0
            if t.msSamp > 1:
                save.sample.mapToArray = False
                save.sample.sampleIndex = 0
            saved = controller.SaveTexture(save, os.path.join(OUT, name)) == rd.ResultCode.Succeeded
        say(i, "events", first, "-", last, "draws", count, "target", int(target), desc, name if saved else "-")
    # Copies, blits and resolves: their destination right after them (resolved scene, downsample chain...).
    say("copies")
    n = 0
    for a in actions:
        if not (a.flags & (rd.ActionFlags.Copy | rd.ActionFlags.Resolve)):
            continue
        dst = a.copyDestination
        t = textures.get(dst)
        if not t or t.width < 32 or t.height < 16:
            continue
        name = "copy_%03d_e%d_%dx%d.png" % (n, a.eventId, t.width, t.height)
        controller.SetFrameEvent(a.eventId, True)
        save = rd.TextureSave()
        save.resourceId = dst
        save.destType = rd.FileType.PNG
        save.alpha = rd.AlphaMapping.Discard
        save.mip = 0
        save.slice.sliceIndex = 0
        ok = controller.SaveTexture(save, os.path.join(OUT, name)) == rd.ResultCode.Succeeded
        say(n, "event", a.eventId, a.GetName(sfile), "dst", int(dst), "%dx%d %s" % (t.width, t.height, t.format.Name()),
            name if ok else "-")
        n += 1
        if n >= 120:
            break
    controller.Shutdown()
    cap.Shutdown()
    say("done")
except Exception:
    say("ERROR", traceback.format_exc())
log.close()
os._exit(0)
