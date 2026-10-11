// Dark Souls II: profile collection of the PGO-instrumented Switch build (DS2_PGO=generate), see ds2_pgo.cpp.
// In every other build these do nothing.

#pragma once

namespace ds2::pgo {

// Picks this session's file (pgo/ds2_pgo_<N>.stream next to the executable) and starts the 2-minute dumps.
void Start();
// Writes every count so far to this session's file (the whole session each time, replacing the previous dump).
void DumpNow();
// The last dump, then the dump thread ends.
void Stop();

}  // namespace ds2::pgo
