// sa2-nx (2026-10-20): banjo-nx's enhancement switches, as Renut's renderer (through banjo-nx) asks for them, answered
// by SA2's (src/cpu/ds2_enhancements.h): a banjo_* key is active when it is on and its group's master is on
// (ds2_cpu_enhancements / ds2_noncpu_enhancements).
#pragma once

#include "cpu/ds2_enhancements.h"

namespace banjo {

enum class EnhancementGroup { kCpu, kNonCpu };

inline bool EnhancementActive(EnhancementGroup group, bool own_key) {
  return own_key && (group == EnhancementGroup::kCpu ? ds2::enh::CpuMaster() : ds2::enh::NonCpuMaster());
}

// The generated code's self-checks: SA2's own 10 s enhancement report gives them (ds2::enh::StartReport).
inline void CodegenChecksReport(int) {}

}  // namespace banjo
