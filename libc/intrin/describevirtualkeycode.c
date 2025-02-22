/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/intrin/kprintf.h"
#include "libc/macros.internal.h"
#include "libc/nt/enum/vk.h"

// clang-format off
static const struct VirtualKeyCodeName {
  uint32_t code;
  const char *name;
} kVirtualKeyCodeNames[] = {
  {kNtVkLbutton, "kNtVkLbutton"},
  {kNtVkRbutton, "kNtVkRbutton"},
  {kNtVkCancel, "kNtVkCancel"},
  {kNtVkMbutton, "kNtVkMbutton"},
  {kNtVkXbutton1, "kNtVkXbutton1"},
  {kNtVkXbutton2, "kNtVkXbutton2"},
  {kNtVkBack, "kNtVkBack"},
  {kNtVkTab, "kNtVkTab"},
  {kNtVkClear, "kNtVkClear"},
  {kNtVkReturn, "kNtVkReturn"},
  {kNtVkShift, "kNtVkShift"},
  {kNtVkControl, "kNtVkControl"},
  {kNtVkMenu, "kNtVkMenu"},
  {kNtVkPause, "kNtVkPause"},
  {kNtVkCapital, "kNtVkCapital"},
  {kNtVkKana, "kNtVkKana"},
  {kNtVkHangul, "kNtVkHangul"},
  {kNtVkJunja, "kNtVkJunja"},
  {kNtVkFinal, "kNtVkFinal"},
  {kNtVkHanja, "kNtVkHanja"},
  {kNtVkKanji, "kNtVkKanji"},
  {kNtVkEscape, "kNtVkEscape"},
  {kNtVkConvert, "kNtVkConvert"},
  {kNtVkNonconvert, "kNtVkNonconvert"},
  {kNtVkAccept, "kNtVkAccept"},
  {kNtVkModechange, "kNtVkModechange"},
  {kNtVkSpace, "kNtVkSpace"},
  {kNtVkPrior, "kNtVkPrior"},
  {kNtVkNext, "kNtVkNext"},
  {kNtVkEnd, "kNtVkEnd"},
  {kNtVkHome, "kNtVkHome"},
  {kNtVkLeft, "kNtVkLeft"},
  {kNtVkUp, "kNtVkUp"},
  {kNtVkRight, "kNtVkRight"},
  {kNtVkDown, "kNtVkDown"},
  {kNtVkSelect, "kNtVkSelect"},
  {kNtVkPrint, "kNtVkPrint"},
  {kNtVkExecute, "kNtVkExecute"},
  {kNtVkSnapshot, "kNtVkSnapshot"},
  {kNtVkInsert, "kNtVkInsert"},
  {kNtVkDelete, "kNtVkDelete"},
  {kNtVkHelp, "kNtVkHelp"},
  {kNtVkLwin, "kNtVkLwin"},
  {kNtVkRwin, "kNtVkRwin"},
  {kNtVkApps, "kNtVkApps"},
  {kNtVkSleep, "kNtVkSleep"},
  {kNtVkNumpad0, "kNtVkNumpad0"},
  {kNtVkNumpad1, "kNtVkNumpad1"},
  {kNtVkNumpad2, "kNtVkNumpad2"},
  {kNtVkNumpad3, "kNtVkNumpad3"},
  {kNtVkNumpad4, "kNtVkNumpad4"},
  {kNtVkNumpad5, "kNtVkNumpad5"},
  {kNtVkNumpad6, "kNtVkNumpad6"},
  {kNtVkNumpad7, "kNtVkNumpad7"},
  {kNtVkNumpad8, "kNtVkNumpad8"},
  {kNtVkNumpad9, "kNtVkNumpad9"},
  {kNtVkMultiply, "kNtVkMultiply"},
  {kNtVkAdd, "kNtVkAdd"},
  {kNtVkSeparator, "kNtVkSeparator"},
  {kNtVkSubtract, "kNtVkSubtract"},
  {kNtVkDecimal, "kNtVkDecimal"},
  {kNtVkDivide, "kNtVkDivide"},
  {kNtVkF1, "kNtVkF1"},
  {kNtVkF2, "kNtVkF2"},
  {kNtVkF3, "kNtVkF3"},
  {kNtVkF4, "kNtVkF4"},
  {kNtVkF5, "kNtVkF5"},
  {kNtVkF6, "kNtVkF6"},
  {kNtVkF7, "kNtVkF7"},
  {kNtVkF8, "kNtVkF8"},
  {kNtVkF9, "kNtVkF9"},
  {kNtVkF10, "kNtVkF10"},
  {kNtVkF11, "kNtVkF11"},
  {kNtVkF12, "kNtVkF12"},
  {kNtVkF13, "kNtVkF13"},
  {kNtVkF14, "kNtVkF14"},
  {kNtVkF15, "kNtVkF15"},
  {kNtVkF16, "kNtVkF16"},
  {kNtVkF17, "kNtVkF17"},
  {kNtVkF18, "kNtVkF18"},
  {kNtVkF19, "kNtVkF19"},
  {kNtVkF20, "kNtVkF20"},
  {kNtVkF21, "kNtVkF21"},
  {kNtVkF22, "kNtVkF22"},
  {kNtVkF23, "kNtVkF23"},
  {kNtVkF24, "kNtVkF24"},
  {kNtVkNumlock, "kNtVkNumlock"},
  {kNtVkScroll, "kNtVkScroll"},
  {kNtVkLshift, "kNtVkLshift"},
  {kNtVkRshift, "kNtVkRshift"},
  {kNtVkLcontrol, "kNtVkLcontrol"},
  {kNtVkRcontrol, "kNtVkRcontrol"},
  {kNtVkLmenu, "kNtVkLmenu"},
  {kNtVkRmenu, "kNtVkRmenu"},
  {kNtVkBrowserBack, "kNtVkBrowserBack"},
  {kNtVkBrowserForward, "kNtVkBrowserForward"},
  {kNtVkBrowserRefresh, "kNtVkBrowserRefresh"},
  {kNtVkBrowserStop, "kNtVkBrowserStop"},
  {kNtVkBrowserSearch, "kNtVkBrowserSearch"},
  {kNtVkBrowserFavorites, "kNtVkBrowserFavorites"},
  {kNtVkBrowserHome, "kNtVkBrowserHome"},
  {kNtVkVolumeMute, "kNtVkVolumeMute"},
  {kNtVkVolumeDown, "kNtVkVolumeDown"},
  {kNtVkVolumeUp, "kNtVkVolumeUp"},
  {kNtVkMediaNextTrack, "kNtVkMediaNextTrack"},
  {kNtVkMediaPrevTrack, "kNtVkMediaPrevTrack"},
  {kNtVkMediaStop, "kNtVkMediaStop"},
  {kNtVkMediaPlayPause, "kNtVkMediaPlayPause"},
  {kNtVkLaunchMail, "kNtVkLaunchMail"},
  {kNtVkLaunchMediaSelect, "kNtVkLaunchMediaSelect"},
  {kNtVkLaunchApp1, "kNtVkLaunchApp1"},
  {kNtVkLaunchApp2, "kNtVkLaunchApp2"},
  {kNtVkOem_1, "kNtVkOem_1"},
  {kNtVkOemPlus, "kNtVkOemPlus"},
  {kNtVkOemComma, "kNtVkOemComma"},
  {kNtVkOemMinus, "kNtVkOemMinus"},
  {kNtVkOemPeriod, "kNtVkOemPeriod"},
  {kNtVkOem_2, "kNtVkOem_2"},
  {kNtVkOem_3, "kNtVkOem_3"},
  {kNtVkGamepadA, "kNtVkGamepadA"},
  {kNtVkGamepadB, "kNtVkGamepadB"},
  {kNtVkGamepadX, "kNtVkGamepadX"},
  {kNtVkGamepadY, "kNtVkGamepadY"},
  {kNtVkGamepadRightShoulder, "kNtVkGamepadRightShoulder"},
  {kNtVkGamepadLeftShoulder, "kNtVkGamepadLeftShoulder"},
  {kNtVkGamepadLeftTrigger, "kNtVkGamepadLeftTrigger"},
  {kNtVkGamepadRightTrigger, "kNtVkGamepadRightTrigger"},
  {kNtVkGamepadDpadUp, "kNtVkGamepadDpadUp"},
  {kNtVkGamepadDpadDown, "kNtVkGamepadDpadDown"},
  {kNtVkGamepadDpadLeft, "kNtVkGamepadDpadLeft"},
  {kNtVkGamepadDpadRight, "kNtVkGamepadDpadRight"},
  {kNtVkGamepadMenu, "kNtVkGamepadMenu"},
  {kNtVkGamepadView, "kNtVkGamepadView"},
  {kNtVkGamepadLeftThumbstickButton, "kNtVkGamepadLeftThumbstickButton"},
  {kNtVkGamepadRightThumbstickButton, "kNtVkGamepadRightThumbstickButton"},
  {kNtVkGamepadLeftThumbstickUp, "kNtVkGamepadLeftThumbstickUp"},
  {kNtVkGamepadLeftThumbstickDown, "kNtVkGamepadLeftThumbstickDown"},
  {kNtVkGamepadLeftThumbstickRight, "kNtVkGamepadLeftThumbstickRight"},
  {kNtVkGamepadLeftThumbstickLeft, "kNtVkGamepadLeftThumbstickLeft"},
  {kNtVkGamepadRightThumbstickUp, "kNtVkGamepadRightThumbstickUp"},
  {kNtVkGamepadRightThumbstickDown, "kNtVkGamepadRightThumbstickDown"},
  {kNtVkGamepadRightThumbstickRight, "kNtVkGamepadRightThumbstickRight"},
  {kNtVkGamepadRightThumbstickLeft, "kNtVkGamepadRightThumbstickLeft"},
  {kNtVkOem_4, "kNtVkOem_4"},
  {kNtVkOem_5, "kNtVkOem_5"},
  {kNtVkOem_6, "kNtVkOem_6"},
  {kNtVkOem_7, "kNtVkOem_7"},
  {kNtVkOem_8, "kNtVkOem_8"},
  {kNtVkOemAx, "kNtVkOemAx"},
  {kNtVkOem_102, "kNtVkOem_102"},
  {kNtVkIcoHelp, "kNtVkIcoHelp"},
  {kNtVkIco_00, "kNtVkIco_00"},
  {kNtVkProcesskey, "kNtVkProcesskey"},
  {kNtVkIcoClear, "kNtVkIcoClear"},
  {kNtVkPacket, "kNtVkPacket"},
  {kNtVkOemReset, "kNtVkOemReset"},
  {kNtVkOemJump, "kNtVkOemJump"},
  {kNtVkOemPa1, "kNtVkOemPa1"},
  {kNtVkOemPa2, "kNtVkOemPa2"},
  {kNtVkOemPa3, "kNtVkOemPa3"},
  {kNtVkOemWsctrl, "kNtVkOemWsctrl"},
  {kNtVkOemCusel, "kNtVkOemCusel"},
  {kNtVkOemAttn, "kNtVkOemAttn"},
  {kNtVkOemFinish, "kNtVkOemFinish"},
  {kNtVkOemCopy, "kNtVkOemCopy"},
  {kNtVkOemAuto, "kNtVkOemAuto"},
  {kNtVkOemEnlw, "kNtVkOemEnlw"},
  {kNtVkOemBacktab, "kNtVkOemBacktab"},
  {kNtVkAttn, "kNtVkAttn"},
  {kNtVkCrsel, "kNtVkCrsel"},
  {kNtVkExsel, "kNtVkExsel"},
  {kNtVkEreof, "kNtVkEreof"},
  {kNtVkPlay, "kNtVkPlay"},
  {kNtVkZoom, "kNtVkZoom"},
  {kNtVkNoname, "kNtVkNoname"},
  {kNtVkPa1, "kNtVkPa1"},
  {kNtVkOemClear, "kNtVkOemClear"},
};
// clang-format on

const char *(DescribeVirtualKeyCode)(char buf[32], uint32_t x) {
  for (int i = 0; i < ARRAYLEN(kVirtualKeyCodeNames); ++i) {
    if (x == kVirtualKeyCodeNames[i].code) {
      return kVirtualKeyCodeNames[i].name;
    }
  }
  ksnprintf(buf, 32, "%#x[%#lc]", x, x);
  return buf;
}
