# Native menu pointer preview — preserved work in progress

Single Player baseline 9d850b4 is user-confirmed: hover highlights, clicks activate entries, and empty-space clicks do nothing. Its patch, executable and complete source bundle are preserved separately.

The expanded executable was reported as "menus work". Screen-by-screen coverage was not specified. Its original binary is preserved under `Expanded menu playtest build` in this chat's outputs. It has two user-reported slider bugs; see [MENU-SLIDER-BUGS.md](MENU-SLIDER-BUGS.md). Fixes are deferred at the user's request.

The current source also includes later, unplayed help-caption refresh and Win32 button-capture refinements. Those changes were not installed into or copied over the executable the user tested. This source checkpoint is work in progress, not a finished release.

Mapped source coverage: campaign main and Single Player menus, skill/bonuses, native options root, gameplay/controller/sound options, native confirmation dialogs, pause root and pause advanced options. Plain label bounds come from the native font metrics/alignment. Dialogs use their actual presentation captions; ownership prevents click-through. Sensitivity/volume tracks and toggle arrows use native image vertices. Values are adjusted through original callbacks. Brightness is not mapped yet. Other unmapped image/layout/chapter screens retain their existing controls.

The campaign capture path uses native connection/player and menu state rather than file counts. Its full gameplay/pause/loading transition matrix remains unverified. Multiplayer capture is unchanged.

Checks passed: full campaign build with local generated sources and fifteen level libraries; standalone coordinate/rectangle/modal-slot/slider mapping checks; actual host module exercised against simulated guest memory and callbacks for menu activation, blank space, modal priority/reconfiguration, pause routing, sliders and cancellation; multiplayer source generation. The simulated callbacks did not emulate native arrow-repeat handling and therefore missed the reported repeating sensitivity bug. No full multiplayer build was run.

Expanded runtime boot/rendering was observed in logs, around 30 fps. Desktop capture timed out on both attempts; the agent did not perform blind UI input or independently validate the new interactions visually. Original game/settings were not replaced.

Future focused playtest: verify brightness after its own patch; verify one-click sensitivity change stops after release and test dragging, release outside the window, focus loss and keyboard/controller holds. Recheck baseline Single Player behavior. Do not declare either reported bug fixed until live tested.
