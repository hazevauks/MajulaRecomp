// Recordings of the game image for testing, taken from the presenter's guest output (what the game presents), so
// they work with the window minimized and nothing can cover them. Same file format as the sa2-nx / banjo-nx recorder
// (captures/video_<s>s.rgba + .txt), so tools/make_video.sh and tools/frames_to_png.py read it.
//
// Stage 1 version: frames are taken on a wall-clock period, not at the game's flip (the flip function of this game's
// Direct3D is not located yet), so there is no lockstep mode and no per-frame hash.

#pragma once

#include <functional>

namespace rex::ui {
class Presenter;
struct RawImage;
}

namespace ds2::capture {

// Starts the recorder thread when ds2_video_s > 0.
void Start(std::function<rex::ui::Presenter*()> get_presenter);

// The same with another source of the presented frame (sa2-nx 2026-10-20: Renut's renderer, which has no SDK
// presenter: renut::gpu::CaptureFrame). Its Swap counts g_ds2_game_frames. No lockstep mode (that is the PM4 ring's).
void StartWithSource(std::function<bool(rex::ui::RawImage&)> grab);

// Lockstep with Renut's renderer (sa2-nx 2026-10-20; ds2_video_lockstep, ds2_video_every = 1): its Swap hook asks for
// the capture of the frame it is queueing (RenutLockstepOn), waits for that frame's image and hands it over here with
// the game frame it ends; this returns once the recorder has taken it, so no frame is skipped or taken late and two
// runs label the same frames the same, however loaded the PC is.
bool RenutLockstepOn();
void RenutLockstepFrame(uint64_t game_frame, const rex::ui::RawImage* image);

// Stops it (writes what it has, if anything).
void Stop();

}  // namespace ds2::capture
