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
}

namespace ds2::capture {

// Starts the recorder thread when ds2_video_s > 0.
void Start(std::function<rex::ui::Presenter*()> get_presenter);

// Stops it (writes what it has, if anything).
void Stop();

}  // namespace ds2::capture
