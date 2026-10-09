# banjo-nx: describes draws of a RenderDoc capture (run inside qrenderdoc, like rdc_passes.py).
#   RDC_IN = capture, RDC_OUT = folder for draws.txt, RDC_EVENTS = "first-last" range of event ids (default all)
# For each draw: the GPU label in front of it ((VS << 16) | PS numbers), colour/depth targets, the textures the
# pixel shader reads (resource, size, format) and blend state of target 0.
import os
import traceback

import renderdoc as rd

IN = os.environ.get("RDC_IN")
OUT = os.environ.get("RDC_OUT")
EVENTS = os.environ.get("RDC_EVENTS", "")
LABEL = os.environ.get("RDC_LABEL", "")  # only draws whose GPU label contains this text
VSONLY = os.environ.get("RDC_VSONLY", "") == "1"  # only draws whose vertex shader reads textures
log = open(os.path.join(OUT, "draws.txt"), "w")


def say(*parts):
    log.write(" ".join(str(p) for p in parts) + "\n")
    log.flush()


def flatten(actions, out):
    for a in actions:
        out.append(a)
        flatten(a.children, out)
    return out


try:
    first, last = (0, 1 << 30)
    if EVENTS:
        first, last = [int(v) for v in EVENTS.split("-")]
    cap = rd.OpenCaptureFile()
    if cap.OpenFile(IN, "", None) != rd.ResultCode.Succeeded:
        raise RuntimeError("open file")
    result = cap.OpenCapture(rd.ReplayOptions(), None)
    status, controller = result if isinstance(result, tuple) else (rd.ResultCode.Succeeded, result)
    sfile = controller.GetStructuredFile()
    textures = {t.resourceId: t for t in controller.GetTextures()}
    actions = flatten(controller.GetRootActions(), [])
    last_label = ""
    for a in actions:
        if a.flags & rd.ActionFlags.SetMarker:
            last_label = a.GetName(sfile)
            continue
        if not (a.flags & rd.ActionFlags.Drawcall) or a.eventId < first or a.eventId > last:
            continue
        if LABEL and LABEL not in last_label:
            continue
        controller.SetFrameEvent(a.eventId, True)
        state = controller.GetPipelineState()
        if VSONLY:
            has = False
            try:
                for used in state.GetReadOnlyResources(rd.ShaderStage.Vertex, True):
                    for d in used.descriptors if hasattr(used, "descriptors") else [used.descriptor]:
                        t = textures.get(d.resource)
                        if t and t.width > 1:
                            has = True
            except Exception:
                pass
            if not has:
                continue
        outs = []
        for o in a.outputs:
            if o != rd.ResourceId.Null():
                t = textures.get(o)
                outs.append("%d(%dx%d %s)" % (int(o), t.width, t.height, t.format.Name()) if t else str(int(o)))
        say("event", a.eventId, "label", last_label, "verts", a.numIndices, "inst", a.numInstances, "out", outs,
            "depth", int(a.depthOut))
        try:
            blends = state.GetColorBlends()
            if len(blends):
                b = blends[0]
                say("   blend", b.enabled, "color", b.colorBlend.source, b.colorBlend.operation, b.colorBlend.destination,
                    "alpha", b.alphaBlend.source, b.alphaBlend.operation, b.alphaBlend.destination,
                    "mask", b.writeMask)
        except Exception as e:
            say("   blend ?", e)
        try:
            for stage, tag in ((rd.ShaderStage.Vertex, "vs"), (rd.ShaderStage.Pixel, "ps")):
                for used in state.GetReadOnlyResources(stage, True):
                    for d in used.descriptors if hasattr(used, "descriptors") else [used.descriptor]:
                        rid = d.resource
                        t = textures.get(rid)
                        if t:
                            extra = ""
                            try:
                                sw = d.swizzle
                                extra += " swz %s%s%s%s" % tuple(str(c).split(".")[-1][0] for c in (sw.red, sw.green, sw.blue, sw.alpha))
                            except Exception as e:
                                extra += " swz ?"
                            try:
                                sm = used.sampler
                                extra += " addr %s/%s filter %s/%s/%s" % (str(sm.addressU).split(".")[-1], str(sm.addressV).split(".")[-1],
                                                                       str(sm.filter.minify).split(".")[-1], str(sm.filter.magnify).split(".")[-1],
                                                                       str(sm.filter.mip).split(".")[-1])
                            except Exception as e:
                                extra += " sampler ?"
                            say("   %s tex" % tag, int(rid), "%dx%d %s" % (t.width, t.height, t.format.Name()), extra)
        except Exception as e:
            say("   ps tex ?", e)
    controller.Shutdown()
    cap.Shutdown()
    say("done")
except Exception:
    say("ERROR", traceback.format_exc())
log.close()
os._exit(0)
