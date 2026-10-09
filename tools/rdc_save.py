# banjo-nx: saves chosen textures of a RenderDoc capture as PNG (run inside qrenderdoc, like rdc_passes.py).
#   RDC_IN = capture, RDC_OUT = output folder, RDC_SAVES = "event:resource:name:channel;..."
#   channel: -1 = RGB, 0..3 = that channel alone as grey (3 = alpha). Writes save.txt with the results.
import os
import traceback

import renderdoc as rd

IN = os.environ.get("RDC_IN")
OUT = os.environ.get("RDC_OUT")
SAVES = os.environ.get("RDC_SAVES", "")
log = open(os.path.join(OUT, "save.txt"), "w")


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
    for item in SAVES.split(";"):
        if not item:
            continue
        event, resource, name, channel = item.split(":")
        event, resource, channel = int(event), int(resource), int(channel)
        if event != current:
            controller.SetFrameEvent(event, True)
            current = event
        t = textures.get(resource)
        if not t:
            say(item, "no such texture")
            continue
        save = rd.TextureSave()
        save.resourceId = t.resourceId
        save.destType = rd.FileType.PNG
        save.alpha = rd.AlphaMapping.Discard
        save.mip = 0
        save.slice.sliceIndex = 0
        save.channelExtract = channel
        if t.msSamp > 1:
            save.sample.mapToArray = False
            save.sample.sampleIndex = 0
        ok = controller.SaveTexture(save, os.path.join(OUT, name + ".png")) == rd.ResultCode.Succeeded
        say(item, "%dx%d %s" % (t.width, t.height, t.format.Name()), "ok" if ok else "FAILED")
    controller.Shutdown()
    cap.Shutdown()
    say("done")
except Exception:
    say("ERROR", traceback.format_exc())
log.close()
os._exit(0)
