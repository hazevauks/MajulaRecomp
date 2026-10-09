# banjo-nx: prints pixel values of capture textures at given events (run inside qrenderdoc, like rdc_passes.py).
#   RDC_IN = capture, RDC_OUT = folder for pick.txt, RDC_PICKS = "event:resource:x:y;event:resource:x:y;..."
import os
import traceback

import renderdoc as rd

IN = os.environ.get("RDC_IN")
OUT = os.environ.get("RDC_OUT")
PICKS = os.environ.get("RDC_PICKS", "")
log = open(os.path.join(OUT, "pick.txt"), "w")


def say(*parts):
    log.write(" ".join(str(p) for p in parts) + "\n")
    log.flush()


try:
    cap = rd.OpenCaptureFile()
    if cap.OpenFile(IN, "", None) != rd.ResultCode.Succeeded:
        raise RuntimeError("open file")
    result = cap.OpenCapture(rd.ReplayOptions(), None)
    status, controller = result if isinstance(result, tuple) else (rd.ResultCode.Succeeded, result)
    textures = {int(t.resourceId): t for t in controller.GetTextures()}
    current = None
    for item in PICKS.split(";"):
        if not item:
            continue
        event, resource, x, y = [int(v) for v in item.split(":")]
        if event != current:
            controller.SetFrameEvent(event, True)
            current = event
        t = textures.get(resource)
        if not t:
            say(item, "no such texture")
            continue
        value = controller.PickPixel(t.resourceId, x, y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
        say("event", event, "res", resource, t.format.Name(), "at", x, y, "->",
            ["%.4f" % v for v in value.floatValue], ["%d" % v for v in value.uintValue])
    controller.Shutdown()
    cap.Shutdown()
    say("done")
except Exception:
    say("ERROR", traceback.format_exc())
log.close()
os._exit(0)
