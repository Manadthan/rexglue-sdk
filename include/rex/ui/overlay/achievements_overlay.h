/**
 * @file        rex/ui/overlay/achievements_overlay.h
 * @brief       Default ImGui achievements overlay dialog.
 *
 * @copyright   Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once

#include <cstdint>
#include <functional>

#include <rex/system/achievement_manager.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/overlay/achievement_icon_cache.h>

namespace rex {
class Runtime;
}  // namespace rex

namespace rex::ui {

class ImmediateDrawer;
class ImmediateTexture;

class AchievementsOverlayDialog : public ImGuiDialog {
 public:
  AchievementsOverlayDialog(ImGuiDrawer* imgui_drawer, ImmediateDrawer* immediate_drawer,
                            rex::Runtime* runtime, rex::system::AchievementManager* achievements);
  ~AchievementsOverlayDialog() override;

  /// Reads the pads for system-UI mode: fills the OR of all pads' button bits
  /// (X_INPUT_GAMEPAD_*) and the strongest left-stick Y. Returns false when no
  /// input could be read.
  using PadPoll = std::function<bool(uint16_t* buttons, int16_t* thumb_ly)>;

  /// System-UI mode, for a dialog the title itself opened (XamShowAchievementsUI).
  /// The dialog then owns itself: it is centred, scrolls with the d-pad or left
  /// stick, and closes (deleting itself, after calling `on_closed`) on pad
  /// B / A / Start / Back or Esc. It only goes away once those buttons are
  /// released again, so the press that closes it never reaches the title.
  /// Call right after construction, on the UI thread.
  void EnableSystemUiMode(PadPoll poll, std::function<void()> on_closed);

 protected:
  void OnDraw(ImGuiIO& io) override;
  void OnClose() override;

 private:
  enum class SystemUiState {
    kOff,          // plain F7 overlay
    kArming,       // waiting for the buttons that opened it to be released
    kOpen,         // listening for a close button
    kWaitRelease,  // hidden, waiting for the close button to be released
  };
  // Returns false once the dialog has asked to be closed.
  bool UpdateSystemUi(ImGuiIO& io);

  SystemUiState system_ui_state_ = SystemUiState::kOff;
  PadPoll pad_poll_;
  std::function<void()> on_closed_;
  float pending_scroll_ = 0.0f;
  bool reset_scroll_ = false;  // ImGui remembers the scroll position per window id

  // Lazily loads icon_path, or icons/<image_id>.png when icon_path is empty.
  ImmediateTexture* GetIcon(const rex::system::AchievementInfo& achievement);

  rex::system::AchievementManager* achievements_ = nullptr;
  AchievementIconCache icon_cache_;
};

}  // namespace rex::ui
