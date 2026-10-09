# banjo-nx: pixel history of the main scene target in a RenderDoc capture (run inside qrenderdoc).
#   RDC_IN = capture, RDC_OUT = folder for history.txt, RDC_PIXELS = "x:y;x:y" (in the scene target's pixels)
# The scene target is the colour target of the pass with the most draws. For each pixel, every event that touched
# it is listed with the GPU label in front of the draw (futari_native_gpu_labels: (VS << 16) | PS numbers),
# the shader output and the value after the event.
import os
import traceback

import renderdoc as rd

IN = os.environ.get("RDC_IN")
OUT = os.environ.get("RDC_OUT")
PIXELS = os.environ.get("RDC_PIXELS", "300:200")
log = open(os.path.join(OUT, "history.txt"), "w")


def say(*parts):
    log.write(" ".join(str(p) for p in parts) + "\n")
    log.flush()


def flatten(actions, out):
    for a in actions:
        out.append(a)
        flatten(a.children, out)
    return out


def rgba(v):
    return "(%.3f %.3f %.3f %.3f)" % tuple(v.col.floatValue[:4])


try:
    cap = rd.OpenCaptureFile()
    if cap.OpenFile(IN, "", None) != rd.ResultCode.Succeeded:
        raise RuntimeError("open file")
    result = cap.OpenCapture(rd.ReplayOptions(), None)
    status, controller = result if isinstance(result, tuple) else (rd.ResultCode.Succeeded, result)
    sfile = controller.GetStructuredFile()
    actions = flatten(controller.GetRootActions(), [])
    label_before = {}
    last_label = ""
    for a in actions:
        if a.flags & rd.ActionFlags.SetMarker:
            last_label = a.GetName(sfile)
        elif a.flags & rd.ActionFlags.Drawcall:
            label_before[a.eventId] = last_label
    counts = {}
    for a in actions:
        if a.flags & rd.ActionFlags.Drawcall and len(a.outputs) and a.outputs[0] != rd.ResourceId.Null():
            counts[a.outputs[0]] = counts.get(a.outputs[0], 0) + 1
    target = max(counts, key=counts.get)
    tex = {t.resourceId: t for t in controller.GetTextures()}[target]
    say("scene target", int(target), "%dx%d" % (tex.width, tex.height), tex.format.Name(), "draws", counts[target])
    for item in PIXELS.split(";"):
        x, y = [int(v) for v in item.split(":")]
        say("== pixel", x, y)
        history = controller.PixelHistory(target, x, y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
        for m in history:
            label = label_before.get(m.eventId, "")
            say("  event", m.eventId, "label", label, "passed" if m.Passed() else "failed",
                "shader", rgba(m.shaderOut), "after", rgba(m.postMod))
    controller.Shutdown()
    cap.Shutdown()
    say("done")
except Exception:
    say("ERROR", traceback.format_exc())
log.close()
os._exit(0)
