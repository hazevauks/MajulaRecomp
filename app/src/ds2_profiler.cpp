// ds2-nx (2026-10-29): the on-screen profiler (ds2_profiler.h, docs/profiler-overlay.md).
//
// The figures come from Renut's frame statistics (renut_engine/perf/frame_stats.cpp), which call the banjo::profiler
// functions below (renut_engine/ds2_compat/profiler/profiler.h; sa2-nx left them empty): the game thread at each
// Swap and at each wait for the render thread, the render thread at the end of each frame's work and at each host
// present, the GPU when a frame's timestamps come back. Each goes into its own ring of the last kHistory values,
// under one mutex taken once per event (a few times a frame).
#include "ds2_profiler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <vector>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include "renut_engine/ds2_compat/profiler/profiler.h"

REXCVAR_DEFINE_BOOL(ds2_profiler, false, "DS2",
                    "The on-screen profiler (ds2_profiler.cpp): the frame, game thread, render thread, GPU and present "
                    "times of the last frames, shown from the start. R + Minus shows or hides it at any time on the Switch "
                    "(on the PC this key is the only switch)");

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kHistory = 240;  // frames kept (8 s at 30 fps)
constexpr int kAverage = 60;   // the averages' frames
constexpr float kPlotMaxMs = 66.7f;

enum Series { kFrame, kGame, kRender, kGpu, kPresent, kSeriesCount };

struct SeriesInfo {
  const char* name;
  ImU32 colour;
};
constexpr SeriesInfo kInfo[kSeriesCount] = {
    {"Frame (Swap to Swap)", IM_COL32(255, 140, 200, 255)},
    {"Game thread", IM_COL32(110, 190, 255, 255)},
    {"Render thread", IM_COL32(255, 215, 90, 255)},
    {"GPU", IM_COL32(120, 230, 120, 255)},
    {"Present", IM_COL32(255, 160, 70, 255)},
};

struct Ring {
  float v[kHistory] = {};
  int next = 0, count = 0;
  void Push(float x) {
    v[next] = x;
    next = (next + 1) % kHistory;
    count = std::min(count + 1, kHistory);
  }
  // i = 0 is the oldest kept.
  float At(int i) const { return v[(next - count + i + kHistory) % kHistory]; }
  float Last() const { return count ? At(count - 1) : 0.0f; }
  float Average(int n) const {
    n = std::min(n, count);
    if (!n) return 0.0f;
    double sum = 0.0;
    for (int i = count - n; i < count; ++i) sum += At(i);
    return float(sum / n);
  }
};

struct State {
  std::mutex mutex;
  Ring series[kSeriesCount];
  Clock::time_point last_frame{}, last_present{}, last_log{};
  double waits_ms = 0.0;  // the game thread's waits since its last Swap
  // The 10 s log line's sums.
  double log_sum[kSeriesCount] = {};
  int log_n[kSeriesCount] = {};
  std::vector<float> log_frames;
};

State& S() {
  static State s;
  return s;
}

std::atomic<bool> g_shown{false};
rex::ui::ImGuiDrawer* g_imgui = nullptr;
std::function<void(std::function<void()>)> g_on_ui_thread;
class Window;
Window* g_window = nullptr;  // UI thread

void Add(Series which, float ms) {
  State& s = S();
  s.series[which].Push(ms);
  s.log_sum[which] += ms;
  ++s.log_n[which];
}

const char* Limit(float game, float render, float gpu) {
  const float top = std::max({game, render, gpu});
  if (top <= 0.0f) return "-";
  return top == gpu ? "GPU" : top == render ? "render thread" : "game thread";
}

// Every 10 s, from the game thread's Swap (State's mutex held).
void MaybeLog(State& s, Clock::time_point now) {
  if (s.last_log == Clock::time_point{}) s.last_log = now;
  if (now - s.last_log < std::chrono::seconds(10)) return;
  s.last_log = now;
  auto avg = [&](Series w) { return s.log_n[w] ? float(s.log_sum[w] / s.log_n[w]) : 0.0f; };
  float low = 0.0f;  // the 1 % low: the frame rate of the slowest 1 % of the frames
  if (!s.log_frames.empty()) {
    std::vector<float> f = s.log_frames;
    const size_t k = std::max<size_t>(1, f.size() / 100);
    std::partial_sort(f.begin(), f.begin() + k, f.end(), std::greater<float>());
    double worst = 0.0;
    for (size_t i = 0; i < k; ++i) worst += f[i];
    low = float(1000.0 / (worst / k));
  }
  const float frame = avg(kFrame);
  REXLOG_INFO("[profiler] 10 s: frame {:.2f} ms ({:.1f} fps, 1 % low {:.1f} fps), game thread {:.2f}, render thread "
              "{:.2f}, GPU {:.2f}, present {:.2f} ms; limit: {}",
              frame, frame > 0.0f ? 1000.0f / frame : 0.0f, low, avg(kGame), avg(kRender), avg(kGpu), avg(kPresent),
              Limit(avg(kGame), avg(kRender), avg(kGpu)));
  for (int i = 0; i < kSeriesCount; ++i) {
    s.log_sum[i] = 0.0;
    s.log_n[i] = 0;
  }
  s.log_frames.clear();
}

class Window final : public rex::ui::ImGuiDialog {
 public:
  explicit Window(rex::ui::ImGuiDrawer* imgui) : ImGuiDialog(imgui) {}
  void RequestClose() { Close(); }

 protected:
  void OnClose() override {
    if (g_window == this) g_window = nullptr;
  }

  void OnDraw(ImGuiIO& io) override {
    Ring rings[kSeriesCount];
    {
      State& s = S();
      std::lock_guard lock(s.mutex);
      std::copy(std::begin(s.series), std::end(s.series), rings);
    }
    const float scale = std::max(1.0f, io.DisplaySize.y / 720.0f);
    const float width = 440.0f * scale;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - width - 8.0f * scale, 8.0f * scale), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.78f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("##ds2_profiler", nullptr, flags)) {
      ImGui::SetWindowFontScale(scale * 0.9f);
      ImGui::TextUnformatted("Profiler (R + Minus)");
      DrawPlot(rings, width - ImGui::GetStyle().WindowPadding.x * 2.0f, 120.0f * scale);
      for (int i = 0; i < kSeriesCount; ++i) {
        const float last = rings[i].Last(), average = rings[i].Average(kAverage);
        ImGui::PushStyleColor(ImGuiCol_Text, kInfo[i].colour);
        ImGui::Text("%-20s %6.2f ms (avg %6.2f ms, %5.1f FPS)", kInfo[i].name, last, average,
                    average > 0.0f ? 1000.0f / average : 0.0f);
        ImGui::PopStyleColor();
      }
      ImGui::Text("Limit: %s", Limit(rings[kGame].Average(kAverage), rings[kRender].Average(kAverage),
                                     rings[kGpu].Average(kAverage)));
    }
    ImGui::End();
  }

 private:
  static void DrawPlot(const Ring* rings, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), IM_COL32(10, 10, 14, 200));
    auto y_of = [&](float ms) { return o.y + h - std::min(ms, kPlotMaxMs) / kPlotMaxMs * h; };
    for (float guide : {16.7f, 33.3f, 50.0f}) {  // 60, 30 and 20 fps
      const float y = y_of(guide);
      dl->AddLine(ImVec2(o.x, y), ImVec2(o.x + w, y), IM_COL32(255, 255, 255, 40));
      char label[8];
      std::snprintf(label, sizeof(label), "%.0f", guide);
      dl->AddText(ImVec2(o.x + 2.0f, y - ImGui::GetFontSize()), IM_COL32(255, 255, 255, 110), label);
    }
    std::vector<ImVec2> points;
    for (int sidx = 0; sidx < kSeriesCount; ++sidx) {
      const Ring& r = rings[sidx];
      if (r.count < 2) continue;
      points.clear();
      for (int i = 0; i < r.count; ++i)
        points.emplace_back(o.x + w * float(kHistory - r.count + i) / float(kHistory - 1), y_of(r.At(i)));
      dl->AddPolyline(points.data(), int(points.size()), kInfo[sidx].colour, ImDrawFlags_None, 1.5f);
    }
    ImGui::Dummy(ImVec2(w, h));
  }
};

void ToggleOnUiThread() {
  if (!g_imgui) return;
  if (g_window) {
    g_window->RequestClose();  // deleted after its next Draw (OnClose clears g_window)
    g_shown = false;
  } else {
    g_window = new Window(g_imgui);
    g_shown = true;
  }
}

}  // namespace

namespace ds2::profiler {

void Install(rex::ui::ImGuiDrawer* imgui, std::function<void(std::function<void()>)> on_ui_thread) {
  g_imgui = imgui;
  g_on_ui_thread = std::move(on_ui_thread);
  if (REXCVAR_GET(ds2_profiler) && g_on_ui_thread) g_on_ui_thread(ToggleOnUiThread);
}

void RequestToggle() {
  if (g_on_ui_thread) g_on_ui_thread(ToggleOnUiThread);
}

}  // namespace ds2::profiler

// Renut's frame statistics' calls (renut_engine/ds2_compat/profiler/profiler.h).
namespace banjo::profiler {

void SetGpuFrameMs(double ms) {
  State& s = S();
  std::lock_guard lock(s.mutex);
  Add(kGpu, float(ms));
}

void SetRenderThreadMs(double ms) {
  State& s = S();
  std::lock_guard lock(s.mutex);
  Add(kRender, float(ms));
}

void RecordHostPresent() {
  State& s = S();
  std::lock_guard lock(s.mutex);
  const auto now = Clock::now();
  if (s.last_present != Clock::time_point{})
    Add(kPresent, std::chrono::duration<float, std::milli>(now - s.last_present).count());
  s.last_present = now;
}

void SetRendererInfo(const char*, const char*) {}

void RecordGameFrame() {
  State& s = S();
  std::lock_guard lock(s.mutex);
  const auto now = Clock::now();
  if (s.last_frame != Clock::time_point{}) {
    const float frame = std::chrono::duration<float, std::milli>(now - s.last_frame).count();
    Add(kFrame, frame);
    Add(kGame, std::max(0.0f, frame - float(s.waits_ms)));
    s.log_frames.push_back(frame);
  }
  s.last_frame = now;
  s.waits_ms = 0.0;
  MaybeLog(s, now);
}

void AddGameThreadWait(double ms) {
  State& s = S();
  std::lock_guard lock(s.mutex);
  s.waits_ms += ms;
}

}  // namespace banjo::profiler
