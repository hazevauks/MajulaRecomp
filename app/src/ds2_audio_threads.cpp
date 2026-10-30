// Dark Souls II: the priority of FMOD's mixer thread on the Switch.
//
// The game's sound is FMOD Ex 4.40 (docs/ds2-notes.md): a software mixer on its own thread, "FMOD mixer thread", that
// hands XAudio2 one block of 256 samples (5.33 ms) at a time. FMOD asks the system for a high priority for it
// (FMOD_OS_Thread_Create, sub_82853228, calls SetThreadPriority and picks its hardware thread by the thread's name),
// but on the Switch every guest thread runs at 0x3B, the only priority Horizon time-slices (sdk/src/system/
// xthread.cpp, SwitchPriorityFor): the mixer takes turns, 10 ms at a time, with the game's main thread and the other
// busy guest threads, on three cores the renderer's threads also use. When its turn comes late the block goes out
// late and XAudio2 plays a gap: the "robotic", dragging sound of the twentieth build's console session
// (2026-10-13), where the XMA data was ready more than two blocks late in 20 to 50 % of the ten-second windows.
// nfsmw-nx had the same symptom with its own audio server thread and fixed it the same way
// (docs/audio-and-video.md, "Robotic audio": priority 0x2D, "the one that helped most").
//
// Every FMOD thread starts in FMOD::Thread::callback, sub_8285C200 (the routine FMOD::Thread::initThread,
// sub_8285C480, gives CreateThread: lis r11,-32125; addi r4,r11,-16592), with r3 = the Thread object, whose name
// initThread copied to offset 5 (addi r3,r31,5; strncpy 256). The hook reads the name on the new thread itself and
// raises that thread; the mixer blocks between blocks (it waits for XAudio2), so above the time-sliced band it takes
// the CPU when it has a block to mix and gives it back.

#include <cstdint>
#include <cstring>
#include <string_view>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/ppc.h>

#if REX_PLATFORM_SWITCH
#include <switch.h>
#endif

REXCVAR_DEFINE_INT32(ds2_audio_mixer_priority, REX_PLATFORM_SWITCH != 0 ? 0x2D : 0, "DS2",
                     "Switch: Horizon priority of FMOD's mixer thread (0x1C-0x3A; the renderer's ring runs at 0x2C, "
                     "the SDK's audio threads at 0x2B). 0 = the guest threads' own, 0x3B, where it takes turns with "
                     "the game's busy threads and its blocks go out late (robotic sound). Default 45 (0x2D)")
    .range(0, 0x3B);
REXCVAR_DEFINE_INT32(ds2_audio_stream_priority, 0, "DS2",
                     "Switch: the same for \"FMOD stream thread\" (it feeds the streamed sounds to the mixer). 0 = "
                     "left with the guest threads; to try if music or voices still break up with the mixer raised")
    .range(0, 0x3B);

namespace {

void Raise(const char* name, int32_t priority) {
  if (priority <= 0) {
    REXLOG_INFO("[audio] {} started: priority left with the guest threads", name);
    return;
  }
#if REX_PLATFORM_SWITCH
  const Result result = svcSetThreadPriority(CUR_THREAD_HANDLE, uint32_t(priority));
  REXLOG_INFO("[audio] {}: Horizon priority {:#x} {}", name, priority,
              R_SUCCEEDED(result) ? "set" : "REFUSED by the system");
#else
  REXLOG_INFO("[audio] {}: priority {:#x} asked for (Switch only: nothing changed here)", name, priority);
#endif
}

}  // namespace

// FMOD::Thread::callback(void* thread).
REX_EXTERN(__imp__sub_8285C200);
REX_HOOK_RAW(sub_8285C200) {
  const uint32_t thread = ctx.r3.u32;
  if (thread >= 0x10000 && thread < 0xFFFF0000u - 261) {
    char name[32] = {};
    std::memcpy(name, base + thread + 5, sizeof(name) - 1);
    const std::string_view text(name);
    if (text == "FMOD mixer thread") {
      Raise(name, REXCVAR_GET(ds2_audio_mixer_priority));
    } else if (text == "FMOD stream thread") {
      Raise(name, REXCVAR_GET(ds2_audio_stream_priority));
    } else {
      REXLOG_INFO("[audio] FMOD thread \"{}\" started (priority left as it is)", name);
    }
  }
  __imp__sub_8285C200(ctx, base);
}
