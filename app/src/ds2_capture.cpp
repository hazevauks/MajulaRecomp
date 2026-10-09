// Recordings of the game image (see ds2_capture.h).
//
// From ds2_video_s seconds after start, ds2_video_frames frames are written as they are taken to
// captures/video_<s>s.rgba next to the executable (raw R8G8B8X8, frames back to back; streamed, so a long recording
// does not need RAM) with a .txt beside it (width, height, measured fps, frame count). One frame is taken every
// ds2_video_every / 60 seconds: 1 follows a 60 fps game, 60 gives one frame a second (a boot in a few MB).

#include "ds2_capture.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/ui/presenter.h>

REXCVAR_DEFINE_INT32(ds2_video_s, 0, "DS2",
                     "Record the game image from this many seconds after start, to captures/ next to the executable "
                     "(0 = off; works minimized). tools/frames_to_png.py and tools/make_video.sh read the result")
    .range(0, 3600);
REXCVAR_DEFINE_INT32(ds2_video_frames, 300, "DS2", "Frames in the recording (ds2_video_s)").range(1, 36000);
REXCVAR_DEFINE_BOOL(ds2_video_half, true, "DS2",
                    "Record at half resolution (a quarter of the disk space: every other pixel of every other row)");
REXCVAR_DEFINE_INT32(ds2_video_every, 1, "DS2",
                     "Take one frame every this many sixtieths of a second (60 = one frame a second)")
    .range(1, 3600);

namespace ds2::capture {
namespace {

std::mutex g_mutex;
std::condition_variable g_cv;
bool g_stop = false;
std::thread g_thread;

// Waits until `when` or until Stop(); true when stopped.
bool WaitUntil(std::chrono::steady_clock::time_point when) {
  std::unique_lock lock(g_mutex);
  return g_cv.wait_until(lock, when, [] { return g_stop; });
}

void Record(std::function<rex::ui::Presenter*()> get_presenter, int start_s, int frame_count) {
  const auto started_at = std::chrono::steady_clock::now();
  if (WaitUntil(started_at + std::chrono::seconds(start_s))) {
    return;
  }
  const uint32_t step = REXCVAR_GET(ds2_video_half) ? 2 : 1;
  const auto period = std::chrono::microseconds(int64_t(REXCVAR_GET(ds2_video_every)) * 1000000 / 60);
  const auto folder = rex::filesystem::GetExecutableFolder() / "captures";
  std::error_code ec;
  std::filesystem::create_directories(folder, ec);
  const std::string name = "video_" + std::to_string(start_s) + "s";
  std::ofstream raw(folder / (name + ".rgba"), std::ios::binary | std::ios::trunc);
  if (!raw) {
    REXLOG_WARN("[capture] cannot write {}", rex::path_to_utf8(folder / (name + ".rgba")));
    return;
  }
  std::vector<uint8_t> frame;
  int written = 0;
  uint64_t missed = 0;  // periods with no presenter or no guest output yet
  uint32_t width = 0, height = 0;
  const auto first = std::chrono::steady_clock::now();
  auto next = first;
  REXLOG_INFO("[capture] recording {} frames from {} s, one every {} ms", frame_count, start_s,
              period.count() / 1000);
  const auto write_index = [&] {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - first).count();
    const double fps = seconds > 0 ? double(written) / seconds : 60.0;
    std::ofstream(folder / (name + ".txt"), std::ios::binary | std::ios::trunc)
        << width / step << ' ' << height / step << ' ' << fps << ' ' << written << '\n';
  };
  while (written < frame_count) {
    if (WaitUntil(next)) {
      break;
    }
    next += period;
    rex::ui::Presenter* presenter = get_presenter ? get_presenter() : nullptr;
    rex::ui::RawImage image;
    if (!presenter || !presenter->CaptureGuestOutput(image) || !image.width || !image.height) {
      ++missed;
      continue;
    }
    if (width && (image.width != width || image.height != height)) {
      // One recording keeps one frame size; the game does not change its output size while it runs.
      REXLOG_WARN("[capture] the frame size changed ({}x{} -> {}x{}): recording stopped", width, height,
                  image.width, image.height);
      break;
    }
    width = image.width;
    height = image.height;
    const uint32_t w = width / step, h = height / step;
    frame.resize(size_t(w) * h * 4);
    for (uint32_t y = 0; y < h; ++y) {
      const uint8_t* row = image.data.data() + size_t(y) * step * image.stride;
      uint8_t* out = frame.data() + size_t(y) * w * 4;
      if (step == 1) {
        std::memcpy(out, row, size_t(w) * 4);
      } else {
        for (uint32_t x = 0; x < w; ++x) {
          std::memcpy(out + size_t(x) * 4, row + size_t(x) * step * 4, 4);
        }
      }
    }
    raw.write(reinterpret_cast<const char*>(frame.data()), std::streamsize(frame.size()));
    raw.flush();
    ++written;
    // Rewritten after every frame: a test run is stopped by killing the process, often before the last frame.
    write_index();
  }
  raw.close();
  if (!written) {
    REXLOG_WARN("[capture] no frame recorded ({} periods without a presenter or a guest output)", missed);
    return;
  }
  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - first).count();
  REXLOG_INFO("[capture] video {}: {} frames of {}x{} in {:.2f} s ({} periods without an image) -> {}", name, written,
              width / step, height / step, seconds, missed, rex::path_to_utf8(folder / (name + ".rgba")));
}

}  // namespace

void Start(std::function<rex::ui::Presenter*()> get_presenter) {
  if (REXCVAR_GET(ds2_video_s) <= 0 || g_thread.joinable()) {
    return;
  }
  {
    std::lock_guard lock(g_mutex);
    g_stop = false;
  }
  g_thread = std::thread(Record, std::move(get_presenter), int(REXCVAR_GET(ds2_video_s)),
                         int(REXCVAR_GET(ds2_video_frames)));
}

void Stop() {
  {
    std::lock_guard lock(g_mutex);
    g_stop = true;
  }
  g_cv.notify_all();
  if (g_thread.joinable()) {
    g_thread.join();
  }
}

}  // namespace ds2::capture
