// sa2-nx (2026-10-20): for sa2-nx's own hooks at the Direct3D functions Renut's renderer hooks too, first thing in
// their body. With ds2_renderer = "renut": DS2_RENUT_YIELD_SHARED(sub_X) runs Renut's side (renut_native_sub_X,
// banjo_shared_hooks.h); DS2_RENUT_YIELD(sub_X) runs the game's original (the function Renut leaves to SA2's own
// Direct3D, whose callees it hooks). Nothing in a build without Renut's renderer (DS2_RENUT_RENDERER off).
#pragma once

#if DS2_RENUT_RENDERER
#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/banjo_shared_hooks.h"
#define DS2_RENUT_YIELD_SHARED(subroutine) BANJO_RENUT_YIELD_SHARED(subroutine)
#define DS2_RENUT_YIELD(subroutine) BANJO_RENUT_YIELD(subroutine)
#else
#define DS2_RENUT_YIELD_SHARED(subroutine)
#define DS2_RENUT_YIELD(subroutine)
#endif
