TODO for awmaker — next steps and operational notes
====================================================

This file tracks the open work for the awmaker fork (Window Maker / X11 →
abstraction). The current goal (user decision, 2026-08-13) is to abstract
awmaker away from X so that **screens/monitors can be added and removed
without restarting the window manager**. All functional/robustness bugs
are resolved; the abstraction is the active work. The `<XX-YY>` identifiers
below are the historical tracker codes kept here to stay consistent with
past commits and commit messages.

Come borrowing short: this is a TODO, not a full spec. For the high-level
description of awmaker and its user-facing features, see `README`.

Active roadmap
==============

| ID | Open? | Scope | Issue |
|----|-------|-------|-------|
| WB-05 | open | awmaker only | Virtual-screen count **hardcoded to `1`** (`src/startup.c:477`, `int max = 1`). When `screen_count > 1`, `StartUp` indexes `w_global.vscreens[j]` beyond the single allocated vscreen → latent multi-head bug / OOB. Core unfinished abstraction. |
| CFG-02 | open | config | Persist the **number of virtual screens** in config (make the `virtual_screen` count configurable). Depends on CFG-01 (done) + WB-05. |
| ABST-01 | open | X11 | Introduce a central `Types.h` with true abstract types; stop leaking `Display*`/`Window`/`GC`/`Atom` into public structs. |
| ABST-02 | open | X11 | Replace the pervasive global `Display *dpy` (`src/WindowMaker.h`) with a per-`WScreen` handle. |
| ABST-03 | open | X11 | Abstract the head/geometry layer modeled after `src/xinerama.h`; make screen enumeration not depend on `ScreenCount(dpy)`. |
| WPREF-01 | open | WPrefs | WPrefs.app is still X-coupled (`XOpenDisplay`/`WMCreateScreen(Display*)`); needs to adopt the new device abstraction. |

Frentes shared-code (`awmcommon/`) — all **DONE**: A (awconfig), B
(keybinds_meta), C (xmodifier), D (shortcut_parse, in `src/`). Phase A of
the keybinding unification (CUN-3 wontfix, CUN-4/CUN-5 done) is complete.

Why nothing can hot-plug today
------------------------------
- Two parallel fixed arrays built once at startup:
  `w_global.vscreens[]` (`virtual_screen*`, allocated in `startup_virtual()`
  with the **hardcoded `max = 1`** — WB-05) and `static WScreen **wScreen`
  (sized by `ScreenCount(dpy)`). `StartUp()` binds index `j` to both array AND
  to `vscrno` in `dockedapps_autolaunch` → **array index == vscreen id == X
  enumeration order**; any dynamic reorder breaks this contract.
- **No runtime add/remove path.** No `wScreenDestroy`/`dock_destroy`/
  `drawer_destroy`/workspace/menu teardown anywhere (only `clip_destroy`,
  never called at screen granularity). Once `StartUp()` returns the screen
  set is frozen for the process lifetime.
- Hotplug today = `RRScreenChangeNotify` (event.c:583) triggers
  `WSTATE_RESTARTING` → `Shutdown(...); Restart(...)` → whole WM blinks.
  No in-place add/remove; head geometry is read once and frozen.

Recommended phased plan
-----------------------
- **P1 — groundwork:** CFG-02 + WB-05 (configurable vscreen count, remove
  the `max=1` hardcode); fix the `vscreen_count==1` fast-path; decouple
  array index from vscreen id (stable name/config identity).
- **P2 — teardown:** write the missing `wScreenDestroy`/`dock_destroy`/
  `drawer_destroy`/workspace/menu teardown, keyed off a "screen removed"
  event → enable add/remove.
- **P3 — event plumbing:** convert `RRScreenChangeNotify`→restart into a
  refresh event that re-queries heads (`XRRGetMonitors`), reconstructs
  geometry, creates/destroys screens in place.
- **P4 — element isolation:** isolate the presentation backend per element
  (root menu first, then dock/clip/drawer via a `WDock` vtable, then window
  decoration), each behind an interface modeled on `xinerama.h`.
- **P5 — remove global `dpy`:** per-backend/per-screen display handle
  (ABST-02).

Model to copy for the abstract geometry layer (ABST-03): `src/xinerama.h`
(`WXineramaInfo`, `wGetRectForHead`/`wGetHeadForPoint`/
`wGetHeadForPointerLocation`). Main gap: it caches a frozen snapshot; it
has no teardown/refresh.


Operational notes (keep these — not documented elsewhere)
=========================================================

RST-01 — bounds-safe `wstrconcat`, pending upstream push
-------------------------------------------------------
The bounds-safe `WINGs/string.c` `wstrconcat` fix is applied and committed
**locally in the submodule** as `0a8659de` ("WINGs: make wstrconcat
bounds-safe"): awmaker's submodule ref stays at `17d44e15` and is untouched.
**PENDING: push it upstream** (`git -C third_party/wmaker-crm push origin
master`). Once pushed, remove this note.

Build-tooling — stale `.deps` poison `make` between checkouts
-------------------------------------------------------------
Between commits, stale generated `.deps/*.Po` from a later commit can
poison `make` (the Makefile `include`s them; a residual
`src/.deps/xmodifier.Po` referencing `../awmcommon/xmodifier.c` caused
`No rule to make target` until deleted). `make docker-clean` (git clean
-fX) does **not** remove them, and a full `git clean` additionally broke
manpage generation (`wmaker.1`/`wmsetbg.1`: `Directory nonexistent`, the
`*.in` templates are absent in older commits). Recipe per bisect/checkout
point: delete the generated `.deps/` dirs (and reset `awmcommon`
residuals) but **keep `build-libs/`**, then `./docker/build.sh`.

B4 key-resolution discriminant (do not regress)
-----------------------------------------------
`src/defaults.c init_defaults` resolves keybind `entry->key` from
`keybinds_meta`. The discriminator **must** be
`if (entry->convert == getKeybind && entry->extra_data != NULL)` — not a
`(unsigned long) < WKBD_LAST` numeric test. Reason (B4 regression, fixed
2026-08-15): `extra_data` in `optionList[]` is used both for
`(void*)WKBD_*` keybind rows *and* as pointers to enum/option tables
(`getEnum` rows), whose low 64-bit values false-positive the numeric test
→ `entry->key` overwritten with a wrong defaults key (e.g. "RootMenuKey"),
breaking every per-key default read at startup. The `extra_data != NULL`
guard is needed for `KeychainCancelKey` (the one `getKeybind` row with
`NULL` extra_data, whose `(int)NULL` = 0 would alias WKBD_ROOTMENU).

Resolved items (for reference)
==============================
The full historical picture of fixed bugs and ported upstream features
(constant `WB-xx` / `Fxx` / `CUN-xx` / `8F5`, `8F6` work items) is recorded
in the git commit history for the `awmaker` fork. See `git log --oneline`,
and `README` for the user-facing summary of what awmaker adds.
