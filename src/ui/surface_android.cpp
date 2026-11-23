/**
 * @file        ui/surface_android.cpp
 * @brief       ANativeWindow surface size query.
 *
 * @license     BSD 3-Clause License
 */

#include <rex/ui/surface_android.h>

namespace rex {
namespace ui {

bool AndroidNativeWindowSurface::GetSizeImpl(uint32_t& width_out, uint32_t& height_out) const {
  if (!window_) {
    return false;
  }
  int32_t width = ANativeWindow_getWidth(window_);
  int32_t height = ANativeWindow_getHeight(window_);
  if (width <= 0 || height <= 0) {
    return false;
  }
  width_out = uint32_t(width);
  height_out = uint32_t(height);
  return true;
}

}  // namespace ui
}  // namespace rex
