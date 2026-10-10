// Scripted controller input (see ds2_input_script.h).

#include "ds2_input_script.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

#include <rex/cvar.h>
#include <rex/input/input.h>
#include <rex/input/input_driver.h>
#include <rex/input/input_system.h>
#include <rex/kernel/xboxkrnl/video.h>
#include <rex/logging.h>
#include <rex/runtime.h>

REXCVAR_DEFINE_STRING(ds2_input_script, "", "DS2",
                      "Scripted pad for unattended runs: comma-separated <time>:<buttons> steps (seconds, or f<frame>; "
                      "a range t0-t1 holds, a trailing * presses rapidly). Example: 12:start,15:a,17:a,20-25:lup");

REXCVAR_DEFINE_STRING(ds2_input_live, "", "DS2",
                      "Live pad for exploring unattended: a text file the game rereads when it changes (at most "
                      "every 100 ms). Comma-separated '<buttons> [seconds]' items, buttons joined by '+': "
                      "'lup, a 0.6' holds the left stick up until the file changes and presses A for 0.6 s. An "
                      "empty file releases everything. Adds to ds2_input_script");

namespace ds2::input_script {
namespace {

using rex::input::DeviceId;
using rex::input::DeviceInfo;
using rex::input::X_INPUT_CAPABILITIES;
using rex::input::X_INPUT_KEYSTROKE;
using rex::input::X_INPUT_STATE;
using rex::input::X_INPUT_VIBRATION;
using rex::X_RESULT;
using rex::X_STATUS;

// High, so it never meets the counters of the real drivers.
constexpr DeviceId kScriptDevice = DeviceId(0xF0F0F0F000000001ull);
// Player 2 (steps written <time>:p2:<buttons>). With such steps both scripted pads enumerate as pads (not
// synthetic), player 1's first, so they take the two lowest free slots: guest users 0 and 1 when no real pad is
// plugged in.
constexpr DeviceId kScriptDevice2 = DeviceId(0xF0F0F0F000000002ull);

struct Button {
  const char* name;
  uint32_t bit;  // in the 32-bit step masks: X_INPUT_GAMEPAD_* in the low 16 bits, triggers and sticks above
  uint16_t vk;   // VK_PAD_*
};

constexpr uint32_t kLeftTrigger = 1u << 16, kRightTrigger = 1u << 17;
constexpr uint32_t kLeftUp = 1u << 18, kLeftDown = 1u << 19, kLeftLeft = 1u << 20, kLeftRight = 1u << 21;
constexpr uint32_t kRightUp = 1u << 22, kRightDown = 1u << 23, kRightLeft = 1u << 24, kRightRight = 1u << 25;

constexpr Button kButtons[] = {
    {"a", 0x1000, 0x5800},          {"b", 0x2000, 0x5801},           {"x", 0x4000, 0x5802},
    {"y", 0x8000, 0x5803},          {"rb", 0x0200, 0x5804},          {"lb", 0x0100, 0x5805},
    {"lt", kLeftTrigger, 0x5806},   {"rt", kRightTrigger, 0x5807},   {"up", 0x0001, 0x5810},
    {"down", 0x0002, 0x5811},       {"left", 0x0004, 0x5812},        {"right", 0x0008, 0x5813},
    {"start", 0x0010, 0x5814},      {"back", 0x0020, 0x5815},        {"ls", 0x0040, 0x5816},
    {"rs", 0x0080, 0x5817},         {"lup", kLeftUp, 0x5820},        {"ldown", kLeftDown, 0x5821},
    {"lright", kLeftRight, 0x5822}, {"lleft", kLeftLeft, 0x5823},    {"rup", kRightUp, 0x5830},
    {"rdown", kRightDown, 0x5831},  {"rright", kRightRight, 0x5832}, {"rleft", kRightLeft, 0x5833},
};

struct Step {
  bool frames = false;  // times are presented frames, not seconds
  double from = 0, to = 0;
  bool rapid = false;
  uint32_t buttons = 0;  // Button::bit of every button of the step
  int player = 0;        // 0 = player 1, 1 = player 2
};

bool ParseTime(std::string text, bool& frames, double& out) {
  frames = !text.empty() && (text[0] == 'f' || text[0] == 'F');
  if (frames) {
    text.erase(0, 1);
  }
  char* end = nullptr;
  out = std::strtod(text.c_str(), &end);
  return !text.empty() && end && *end == '\0' && out >= 0;
}

std::vector<Step> Parse(std::string script) {
  std::erase(script, '"');
  std::vector<Step> steps;
  size_t pos = 0;
  while (pos < script.size()) {
    size_t next = script.find_first_of(",;", pos);
    std::string item = script.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
    pos = next == std::string::npos ? script.size() : next + 1;
    std::erase_if(item, [](unsigned char c) { return std::isspace(c); });
    if (item.empty()) {
      continue;
    }
    const size_t colon = item.find(':');
    if (colon == std::string::npos) {
      REXLOG_WARN("[input-script] step '{}' has no ':', ignored", item);
      continue;
    }
    std::string when = item.substr(0, colon), what = item.substr(colon + 1);
    Step step;
    if (what.size() > 3 && (what[0] == 'p' || what[0] == 'P') && what[1] == '2' && what[2] == ':') {
      step.player = 1;
      what.erase(0, 3);
    }
    if (!what.empty() && what.back() == '*') {
      step.rapid = true;
      what.pop_back();
    }
    const size_t dash = when.find('-');
    bool frames_from = false, frames_to = false;
    bool ok = ParseTime(when.substr(0, dash), frames_from, step.from);
    if (ok && dash != std::string::npos) {
      ok = ParseTime(when.substr(dash + 1), frames_to, step.to) && frames_to == frames_from && step.to > step.from;
    } else if (ok) {
      // A single press lasts 150 ms (9 frames).
      step.to = step.from + (frames_from ? 9 : 0.15);
    }
    step.frames = frames_from;
    for (size_t b = 0; ok && b <= what.size();) {
      size_t plus = what.find('+', b);
      std::string name = what.substr(b, plus == std::string::npos ? std::string::npos : plus - b);
      std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
      bool found = false;
      for (const auto& button : kButtons) {
        if (name == button.name) {
          step.buttons |= button.bit;
          found = true;
        }
      }
      ok = found;
      b = plus == std::string::npos ? what.size() + 1 : plus + 1;
    }
    if (!ok || !step.buttons) {
      REXLOG_WARN("[input-script] step '{}' not understood, ignored", item);
      continue;
    }
    steps.push_back(step);
  }
  return steps;
}

class ScriptedInputDriver final : public rex::input::InputDriver {
 public:
  explicit ScriptedInputDriver(std::vector<Step> steps, std::string live = {})
      : InputDriver(nullptr, 0), steps_(std::move(steps)), start_(std::chrono::steady_clock::now()),
        live_path_(std::move(live)) {
    two_players_ = std::any_of(steps_.begin(), steps_.end(), [](const Step& s) { return s.player == 1; });
  }

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }

  void EnumerateDevices(std::vector<DeviceInfo>& out) override {
    DeviceInfo info;
    info.id = kScriptDevice;
    info.name = "ds2 input script";
    info.guid = "ds2-input-script";
    info.synthetic = !two_players_;  // merged into player 1 with whatever else is connected
    out.push_back(info);
    if (two_players_) {
      DeviceInfo info2;
      info2.id = kScriptDevice2;
      info2.name = "ds2 input script (player 2)";
      info2.guid = "ds2-input-script-2";
      info2.synthetic = false;
      out.push_back(info2);
    }
  }

  // 0 / 1 for the scripted pads of players 1 / 2, -1 for anything else.
  int PlayerOf(DeviceId id) const {
    return id == kScriptDevice ? 0 : (id == kScriptDevice2 && two_players_) ? 1 : -1;
  }

  X_RESULT GetDeviceState(DeviceId id, X_INPUT_STATE* out_state) override {
    const int player = PlayerOf(id);
    if (player < 0) {
      return X_ERROR_DEVICE_NOT_CONNECTED;
    }
    std::lock_guard lock(mutex_);
    const uint32_t buttons = Update(player);
    if (out_state) {
      std::memset(out_state, 0, sizeof(*out_state));
      out_state->packet_number = packet_[player];
      out_state->gamepad.buttons = uint16_t(buttons & 0xFFFF);
      out_state->gamepad.left_trigger = buttons & kLeftTrigger ? 0xFF : 0;
      out_state->gamepad.right_trigger = buttons & kRightTrigger ? 0xFF : 0;
      const auto axis = [buttons](uint32_t positive, uint32_t negative) {
        return int16_t(buttons & positive ? 32767 : buttons & negative ? -32768 : 0);
      };
      out_state->gamepad.thumb_lx = axis(kLeftRight, kLeftLeft);
      out_state->gamepad.thumb_ly = axis(kLeftUp, kLeftDown);
      out_state->gamepad.thumb_rx = axis(kRightRight, kRightLeft);
      out_state->gamepad.thumb_ry = axis(kRightUp, kRightDown);
    }
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetDeviceCapabilities(DeviceId id, uint32_t, X_INPUT_CAPABILITIES* out_caps) override {
    if (PlayerOf(id) < 0) {
      return X_ERROR_DEVICE_NOT_CONNECTED;
    }
    if (out_caps) {
      std::memset(out_caps, 0, sizeof(*out_caps));
      out_caps->type = 0x01;      // XINPUT_DEVTYPE_GAMEPAD
      out_caps->sub_type = 0x01;  // XINPUT_DEVSUBTYPE_GAMEPAD
      out_caps->gamepad.buttons = 0xFFFF;
      out_caps->gamepad.left_trigger = 0xFF;
      out_caps->gamepad.right_trigger = 0xFF;
      out_caps->gamepad.thumb_lx = out_caps->gamepad.thumb_ly = int16_t(0x7FFF);
      out_caps->gamepad.thumb_rx = out_caps->gamepad.thumb_ry = int16_t(0x7FFF);
    }
    return X_ERROR_SUCCESS;
  }

  X_RESULT SetDeviceVibration(DeviceId id, X_INPUT_VIBRATION*) override {
    return PlayerOf(id) >= 0 ? X_ERROR_SUCCESS : X_ERROR_DEVICE_NOT_CONNECTED;
  }

  X_RESULT GetDeviceKeystroke(DeviceId id, uint32_t, X_INPUT_KEYSTROKE* out_keystroke) override {
    const int player = PlayerOf(id);
    if (player < 0) {
      return X_ERROR_DEVICE_NOT_CONNECTED;
    }
    std::lock_guard lock(mutex_);
    Update(player);
    auto& queue = keystrokes_[player];
    if (queue.empty()) {
      return X_ERROR_EMPTY;
    }
    if (out_keystroke) {
      *out_keystroke = queue.front();
    }
    queue.pop();
    return X_ERROR_SUCCESS;
  }

 private:
  // Buttons held now; queues a keystroke for every change since the last call and logs it.
  uint32_t Update(int player) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    const double frames = double(rex::kernel::xboxkrnl::VdSwapCount());
    uint32_t buttons = 0;
    for (const auto& step : steps_) {
      if (step.player != player) {
        continue;
      }
      const double now = step.frames ? frames : seconds;
      if (now < step.from || now >= step.to) {
        continue;
      }
      if (step.rapid && std::fmod(now - step.from, step.frames ? 6.0 : 0.1) >= (step.frames ? 3.0 : 0.05)) {
        continue;
      }
      buttons |= step.buttons;
    }
    if (player == 0 && !live_path_.empty()) {
      buttons |= Live(seconds);
    }
    const uint32_t changed = buttons ^ held_[player];
    if (changed) {
      ++packet_[player];
      for (const auto& button : kButtons) {
        const uint32_t bit = button.bit;
        if (!(changed & bit)) {
          continue;
        }
        X_INPUT_KEYSTROKE ks = {};
        ks.virtual_key = button.vk;
        ks.flags = (buttons & bit) ? rex::input::X_INPUT_KEYSTROKE_KEYDOWN : rex::input::X_INPUT_KEYSTROKE_KEYUP;
        keystrokes_[player].push(ks);
        if (buttons & bit) {
          REXLOG_INFO("[input-script] {:.2f} s, frame {}: {}{} down", seconds, uint64_t(frames),
                      player ? "p2 " : "", button.name);
        }
      }
      held_[player] = buttons;
    }
    return buttons;
  }

  // ds2_input_live: the buttons the file holds now. The file is read again when its write time changes; an item
  // with a duration counts from the moment it was read.
  uint32_t Live(double seconds) {
    if (seconds - live_checked_ >= 0.1) {
      live_checked_ = seconds;
      std::error_code ec;
      const auto written = std::filesystem::last_write_time(live_path_, ec);
      if (!ec && written != live_written_) {
        live_written_ = written;
        live_items_.clear();
        std::ifstream file(live_path_, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        std::string all = text.str(), item;
        for (char& c : all) {
          if (c == 10 || c == 13) c = ',';
        }
        std::stringstream items(all);
        while (std::getline(items, item, ',')) {
          std::stringstream words(item);
          std::string names;
          double duration = 0;
          if (!(words >> names)) continue;
          words >> duration;
          uint32_t bits = 0;
          std::transform(names.begin(), names.end(), names.begin(), [](unsigned char c) { return std::tolower(c); });
          std::stringstream each(names);
          std::string name;
          while (std::getline(each, name, '+')) {
            for (const auto& button : kButtons) {
              if (name == button.name) bits |= button.bit;
            }
          }
          if (bits) live_items_.push_back({bits, duration > 0 ? seconds + duration : 1e300});
        }
        REXLOG_INFO("[input-script] {:.2f} s: live file read, {} item(s): {}", seconds, live_items_.size(), all);
      }
    }
    uint32_t buttons = 0;
    for (const auto& [bits, until] : live_items_) {
      if (seconds < until) buttons |= bits;
    }
    return buttons;
  }

  const std::vector<Step> steps_;
  const std::chrono::steady_clock::time_point start_;
  const std::string live_path_;
  std::filesystem::file_time_type live_written_{};
  double live_checked_ = -1;
  std::vector<std::pair<uint32_t, double>> live_items_;  // (buttons, held until this many seconds)
  std::mutex mutex_;
  bool two_players_ = false;
  uint32_t held_[2] = {};
  uint32_t packet_[2] = {};
  std::queue<X_INPUT_KEYSTROKE> keystrokes_[2];
};

}  // namespace

void Install(rex::Runtime* runtime) {
  auto* input = runtime ? static_cast<rex::input::InputSystem*>(runtime->input_system()) : nullptr;
  std::string script = REXCVAR_GET(ds2_input_script);
  std::string live = REXCVAR_GET(ds2_input_live);
  if (!input || (script.empty() && live.empty())) {
    return;
  }
  auto steps = Parse(script);
  if (steps.empty() && live.empty()) {
    return;
  }
  REXLOG_INFO("[input-script] {} step(s): {}{}", steps.size(), script,
              live.empty() ? std::string() : " | live file " + live);
  input->AddDriver(std::make_unique<ScriptedInputDriver>(std::move(steps), std::move(live)));
}

}  // namespace ds2::input_script
