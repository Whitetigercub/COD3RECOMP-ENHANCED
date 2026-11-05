# Campaign Single Player pointer preview

Status: implementation preview; live pointer behavior is unverified.

## Scope and behavior

Only the campaign Single Player submenu (New Game, Continue, Chapter Select) uses native pointer hit detection. Its visible, enabled text labels define the hit areas. Moving the pointer over a label calls the native row selection method. A left press and release on the same enabled label invokes the native menu confirm action once. Right press/release inside the game picture invokes native back. Blank-space clicks do not activate the keyboard selection. Keyboard/controller navigation can retain focus when the pointer stays still. The last focused row remains highlighted when the pointer leaves the labels.

Other frontend menus retain their previous controls. Confirmation dialogs, options, sliders and pause-menu pointer support are future work. Mouse events are blocked from the underlying supported page while its modal/transition locks are active.

Campaign capture uses native connection state, local soldier/playerstate existence and frontend/pause/modal state, replacing the file-count heuristic in the campaign path. Multiplayer retains its prior capture behavior. Gameplay/pause/loading transitions still need live verification.

## Source evidence

Generated sources were inspected, not edited. Local generated campaign and level sources and private build dependencies were copied from E:\Games\COD3RECOMP\source into an isolated clone. Nothing was installed over the game.

- Menu manager: 0x82A8DF2C. Frontend system +28; pause system +32; per-local modal systems +200.
- Frontend system: active byte +28, signed current menu +12, menu pointer array +4, count +20. Index 2 and vtable 0x82071EC4 identify the Single Player submenu.
- Menu: child array +4, selected signed short +34, count +38, lock +48, busy +76. Child vtable 0x82071034; text pointer +16, selected +21, disabled +22.
- Plain label vtable 0x82070B54. Visible bit 4 at +16; anchor +44/+48; selected scales +56/+60, unselected +80/+84. Alignment flags at +108.
- Native metrics 0x824CFBE8 and 0x824CFB48 match plain-text draw 0x824CFD58 and alignment 0x824BF4A8. Layout uses a 640x480 logical canvas. Host conversion matches the floor-rounded 16:9 presentation viewport, including letterboxing.
- Row selection: 0x824CE558. Native action dispatcher: 0x824C48B8, action 64 confirm, action 128 back. No D-pad motion or synthetic pad buttons are used by this pointer path.
- Frontend update hook: 0x824CF060. Frame hook 0x82511168 cancels queued presses when the frontend closes.
- Native modal current indexes are signed: -1 idle, 0 first dialog. See 0x824CF060 and constructor 0x824ECEE0.
- Native usercmd production requires connection state at 0x829C3F08 == 2 (0x82510EF0 / 0x82510980). Local entity slot 1 is 0x82A2DCD8; playerstate pointer +528 is also used by the existing co-op bridge.

## Validation

Full campaign executable build passed with Clang 19.1.5, using the locally available generated sources and all fifteen level libraries. Standalone helper checks passed for modal sentinels, 16:9/4:3/ultrawide coordinate mapping, letterbox rejection, half-open rectangle edges, invalid rectangles and press/release cancellation. Multiplayer host-source generation passed; a full multiplayer executable build was not run.

An isolated executable booted and logged frontend rendering at approximately 30 fps. Desktop screenshot capture failed twice (FrameArrived/window capture timeout); no blind input was issued. Therefore hover, native activation, disabled Continue handling, keyboard/controller coexistence, DPI mapping and gameplay capture transitions are NOT runtime-validated. The preview is not ready to call complete or merge without that playtest.

## Focused playtest

1. Launch the separate preview. Use Enter/Space through startup, then keyboard navigation to open Single Player. Existing installation settings and saves remain separate from the preview copies.
2. Move over New Game, Continue and Chapter Select. Confirm the row under the pointer highlights. Continue should reject interaction when disabled.
3. Click blank space: no activation. Click Chapter Select: it should open exactly once. Use keyboard back to return; then test right-click back from the Single Player page.
4. Press on one label, drag to another and release: neither should activate. Press on blank space and release on a label: no activation. Test focus loss and F11 while a press is held.
5. Keep the pointer still and use arrows/Enter or a controller. Confirm pointer code does not continually override that selection.
6. Resize to a taller/wider window; test the visible labels and letterbox bars. Check non-default Windows DPI.
7. Load gameplay and test automatic capture, Escape pause/release, controller pause/release, resume/capture, focus loss, F11 and return to frontend. Pause menus themselves still use keyboard/controller navigation.

Enable COD3_MENUMOUSELOG=1 for native hover/activation/back event logs. Saves, INI and logs are beside the isolated executable. Do not copy the preview over the installed executable until runtime review is complete.
