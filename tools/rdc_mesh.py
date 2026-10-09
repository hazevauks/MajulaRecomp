# banjo-nx: vertex shader output of chosen draws of a RenderDoc capture (run inside qrenderdoc, like rdc_passes.py).
#   RDC_IN = capture, RDC_OUT = folder for mesh.txt, RDC_LABEL = text the GPU label must contain (VS n77/ ...),
#   RDC_MAX = most draws to describe (default 6)
# For each draw: how many vertices, position range in clip space and in NDC (x/w, y/w, z/w), NaN / w <= 0 counts,
# the first vertices' positions and the first vertex input buffer words.
import math
import os
import struct
import traceback

import renderdoc as rd

IN = os.environ.get("RDC_IN")
OUT = os.environ.get("RDC_OUT")
LABEL = os.environ.get("RDC_LABEL", "")
MAX = int(os.environ.get("RDC_MAX", "6"))
log = open(os.path.join(OUT, "mesh.txt"), "w")


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
    if cap.OpenFile(IN, "", None) != rd.ResultCode.Succeeded:
        raise RuntimeError("open file")
    result = cap.OpenCapture(rd.ReplayOptions(), None)
    status, controller = result if isinstance(result, tuple) else (rd.ResultCode.Succeeded, result)
    sfile = controller.GetStructuredFile()
    actions = flatten(controller.GetRootActions(), [])
    last_label = ""
    done = 0
    for a in actions:
        if a.flags & rd.ActionFlags.SetMarker:
            last_label = a.GetName(sfile)
            continue
        if not (a.flags & rd.ActionFlags.Drawcall) or LABEL not in last_label:
            continue
        controller.SetFrameEvent(a.eventId, True)
        say("event", a.eventId, last_label, "indices", a.numIndices)
        mesh = controller.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)
        if mesh.vertexResourceId == rd.ResourceId.Null():
            say("   no post-VS data")
        else:
            n = mesh.numIndices
            data = controller.GetBufferData(mesh.vertexResourceId, mesh.vertexByteOffset, 0)
            stride = mesh.vertexByteStride
            count = min(len(data) // stride, 65536) if stride else 0
            nan = 0
            wneg = 0
            lo = [1e30] * 3
            hi = [-1e30] * 3
            firsts = []
            for v in range(count):
                x, y, z, w = struct.unpack_from("4f", data, v * stride)
                if any(math.isnan(c) for c in (x, y, z, w)):
                    nan += 1
                    continue
                if w <= 0:
                    wneg += 1
                    continue
                ndc = (x / w, y / w, z / w)
                for i in range(3):
                    lo[i] = min(lo[i], ndc[i])
                    hi[i] = max(hi[i], ndc[i])
                if len(firsts) < 4:
                    firsts.append("(%.3g %.3g %.3g %.3g)" % (x, y, z, w))
            say("   post-VS vertices", count, "stride", stride, "NaN", nan, "w<=0", wneg,
                "ndc x %.3f..%.3f y %.3f..%.3f z %.4f..%.4f" % (lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
            say("   first", " ".join(firsts))
        state = controller.GetPipelineState()
        try:
            vbs = state.GetVBuffers()
            for i, vb in enumerate(vbs[:2]):
                if vb.resourceId == rd.ResourceId.Null():
                    continue
                raw = controller.GetBufferData(vb.resourceId, vb.byteOffset, min(64, vb.byteSize or 64))
                words = struct.unpack_from("%df" % (len(raw) // 4), raw)
                say("   vb%d stride %d:" % (i, vb.byteStride), " ".join("%.4g" % w for w in words[:12]))
        except Exception as e:
            say("   vb ?", e)
        done += 1
        if done >= MAX:
            break
    controller.Shutdown()
    cap.Shutdown()
    say("done")
except Exception:
    say("ERROR", traceback.format_exc())
log.close()
os._exit(0)
