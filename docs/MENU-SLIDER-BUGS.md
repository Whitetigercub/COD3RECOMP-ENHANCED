# Deferred menu slider bugs

The user confirmed Single Player hover, click activation and empty-space rejection in baseline 9d850b4. That checkpoint, patch, complete source bundle and executable remain in `Single Player confirmed baseline 9d850b4`.

The expanded preview was then reported as "menus work". Exact screen-by-screen coverage was not specified. Its original executable is preserved in `Expanded menu playtest build/CoD3.exe`, with its SHA-256 and pointer-event log. The installed game executable and settings were not replaced.

## 1. Game brightness slider does not work

User clarified this is the game's own brightness/calibration screen, not the F11 Graphics slider. Pointer adjustment does not work. The expanded implementation does not yet recognize frontend menu index 8. Do not claim brightness support.

Follow-up: map the gamma menu's real slider/image geometry, range, adjustment and apply paths, then add only that control. Source leads: gamma panel `SP_OP_screen_gamma.panel`, text/image setup 0x824E30F8 and 0x824E3038, gamma help 0x824D26A0. No brightness fix has been implemented or live tested.

## 2. Control sensitivity sliders continue changing after one click

User reports that clicking a mouse sensitivity slider once can leave it decreasing or increasing on its own.

Source-level lead: the pointer path sends native direction action 16/32 through 0x824C48B8. Its menu pre-handler 0x824C56A0 sets native repeat state at menu+50 and the delay at menu+28. The native update 0x824C5800 repeats that stored action. The matching native release handler 0x824C5650 resets menu+50 to 255. The current pointer dispatch does not pair each discrete action with that release handler. This is a strong source-level explanation, not a runtime-verified fix.

Follow-up: a small press/release correction, preserving actual keyboard/controller holds; cover click, drag, release outside the window, row change, dialog transition and focus loss. Add a test that advances native repeat/update behavior after release. The current simulated-callback test did not emulate the game's repeat pre-handler, so its passing result did not exclude this bug.

## Stopping point

At the user's request, both slider fixes are deferred. No further implementation or major rewrite is authorized by this stopping note. Resume when requested with a focused patch for these two bugs.

A later source state contains unplayed caption-refresh and Win32 button-capture refinements. It has not replaced the expanded executable the user tested. Save those changes separately and distinguish them from the confirmed executable. Desktop capture continued to time out; new visual behavior was not independently verified by the agent.
