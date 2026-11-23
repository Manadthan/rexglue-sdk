/**
 * @file        rex/core/fiber_android.cpp
 * @brief       Android (AArch64) backend for rex::thread::Fiber.
 *
 * Bionic does not provide getcontext/makecontext/swapcontext, so fibers switch
 * stacks directly. Only the AAPCS64 callee-saved state is preserved across a
 * switch (x19-x30, sp, d8-d15): a switch is an ordinary function call, so the
 * caller already treats everything else as clobbered.
 *
 * @license     BSD 3-Clause License
 */

#include <rex/platform.h>
#if REX_PLATFORM_ANDROID

#include <rex/thread/fiber.h>

#include <cassert>
#include <cstdlib>
#include <cstring>

#if !defined(__aarch64__)
#error "fiber_android.cpp only implements AArch64"
#endif

extern "C" {
// Saves the callee-saved state on the current stack, stores sp in *save_sp,
// then switches to new_sp and restores the state found there.
void rex_fiber_switch(void** save_sp, void* new_sp);
// Entry stub a fresh fiber "returns" into on its first switch.
void rex_fiber_start();
void rex_fiber_entry() {
  rex::thread::Fiber::Trampoline();
  // A fiber entry must never return: there is no context to go back to.
  std::abort();
}
}

// Frame layout (176 bytes, 16-byte aligned):
//   [0..95]    x19..x30 (x29 = fp, x30 = lr)
//   [96..159]  d8..d15
//   [160..175] padding
__asm__(
    ".text\n"
    ".balign 16\n"
    ".globl rex_fiber_switch\n"
    ".type rex_fiber_switch, %function\n"
    "rex_fiber_switch:\n"
    "  sub sp, sp, #176\n"
    "  stp x19, x20, [sp, #0]\n"
    "  stp x21, x22, [sp, #16]\n"
    "  stp x23, x24, [sp, #32]\n"
    "  stp x25, x26, [sp, #48]\n"
    "  stp x27, x28, [sp, #64]\n"
    "  stp x29, x30, [sp, #80]\n"
    "  stp d8, d9, [sp, #96]\n"
    "  stp d10, d11, [sp, #112]\n"
    "  stp d12, d13, [sp, #128]\n"
    "  stp d14, d15, [sp, #144]\n"
    "  mov x2, sp\n"
    "  str x2, [x0]\n"
    "  mov sp, x1\n"
    "  ldp x19, x20, [sp, #0]\n"
    "  ldp x21, x22, [sp, #16]\n"
    "  ldp x23, x24, [sp, #32]\n"
    "  ldp x25, x26, [sp, #48]\n"
    "  ldp x27, x28, [sp, #64]\n"
    "  ldp x29, x30, [sp, #80]\n"
    "  ldp d8, d9, [sp, #96]\n"
    "  ldp d10, d11, [sp, #112]\n"
    "  ldp d12, d13, [sp, #128]\n"
    "  ldp d14, d15, [sp, #144]\n"
    "  add sp, sp, #176\n"
    "  ret\n"
    ".size rex_fiber_switch, .-rex_fiber_switch\n"
    ".balign 16\n"
    ".globl rex_fiber_start\n"
    ".type rex_fiber_start, %function\n"
    "rex_fiber_start:\n"
    "  mov x29, xzr\n"
    "  mov x30, xzr\n"
    "  bl rex_fiber_entry\n"
    "  brk #0\n"
    ".size rex_fiber_start, .-rex_fiber_start\n");

namespace rex::thread {

namespace {
constexpr size_t kSwitchFrameSize = 176;
}

thread_local Fiber* Fiber::tls_current_ = nullptr;

Fiber* Fiber::ConvertCurrentThread() {
  auto* f = new Fiber();
  f->is_thread_fiber_ = true;  // sp_ is filled in by the first switch away.
  tls_current_ = f;
  return f;
}

Fiber* Fiber::Create(size_t stack_size, void (*entry)(void*), void* arg) {
  if (stack_size < 16 * 1024) {
    stack_size = 16 * 1024;
  }
  auto* f = new Fiber();
  f->entry_ = entry;
  f->arg_ = arg;
  f->stack_.resize(stack_size);

  uintptr_t top = reinterpret_cast<uintptr_t>(f->stack_.data() + f->stack_.size());
  top &= ~uintptr_t(15);
  uintptr_t sp = top - kSwitchFrameSize;
  auto* frame = reinterpret_cast<uint64_t*>(sp);
  std::memset(frame, 0, kSwitchFrameSize);
  frame[11] = reinterpret_cast<uint64_t>(&rex_fiber_start);  // x30 (lr)
  f->sp_ = reinterpret_cast<void*>(sp);
  return f;
}

void Fiber::Trampoline() {
  // tls_current_ was updated by SwitchTo before the switch landed here.
  Fiber* f = tls_current_;
  f->entry_(f->arg_);
}

void Fiber::SwitchTo(Fiber* target) {
  Fiber* from = tls_current_;
  tls_current_ = target;
  rex_fiber_switch(&from->sp_, target->sp_);
}

void Fiber::Destroy() {
  if (is_thread_fiber_) {
    tls_current_ = nullptr;
  } else {
    assert(this != tls_current_ && "Destroy called on the currently running fiber");
  }
  delete this;
}

}  // namespace rex::thread

#endif  // REX_PLATFORM_ANDROID
