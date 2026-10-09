// SA2 - native renderer: access to the shader library the native graphics system has loaded.

#pragma once

namespace ds2::native {

class ShadersNative;

// The library the native graphics system loaded (nullptr until then), from any thread. Used for pipeline
// prewarming (ds2_native_draws.cpp).
const ShadersNative* ActiveLibrary();

}  // namespace ds2::native
