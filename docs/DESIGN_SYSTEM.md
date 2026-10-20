# PaperNX native design system — v0.4-origin design used by v0.9.1

SDL2 drawing, SDL_ttf typography and MuPDF document rendering remain native. Sources: `ui.hpp`, `theme.hpp`, `orientation.hpp`, `focus.hpp`, `typography.cpp`, `main.cpp` and screen include fragments. No web engine/framework.

Shared geometry: 56-pixel touch target, 16 gap, 12 radius, 64 settings cadence. `ui::Tokens` provides title30, section24, label22, metadata18, description17; spacing8/16/24, border1/focus3, header80/footer64, dialog maximum920 and portrait cover154×218. These are logical output pixels; inspect clipping at 640×480 and portrait rather than assuming density-independent scaling.

Noto Sans Regular and OFL Noto Sans SemiBold supply actual distinct UI weights. Noto Serif remains for reading. Button font metrics drive vertical placement; UTF-8 truncation avoids broken Turkish characters. Reader typography is independent of UI size. Text measurement and raster texture caches must be bounded and reused; renderer-thread texture upload only.

`ThemePalette` centralizes background, foreground, muted, accent, card, border and header. The current unified reader chrome follows active document appearance, and the library restores the global palette. Legacy Follow Application/Custom UI policy fields remain readable for migrations but are **not separate visible v0.9.0 choices**. PDF pixel filtering does not directly filter UI widgets. Nine paper presets use RGB overrides without renumbering historical persistence identifiers.

`ui::button`, `ui::settingsRow`, `StepperLayout`, surfaces and focus rings share geometry for hit testing. Numeric rows have left label and independent right decrement/value/increment. Focus adds a three-pixel outline; editing adds an underline; selected and pressed states remain separate. Destructive text uses contrast-aware red. New components must not implement focus solely by hue or by toggling the preference.

`FocusManager` stores logical context/item IDs, geometric movement and per-context memory. Modal context isolates focus. Settings navigation and editing have separate states. Screen composition must register every actionable control, reveal it when focused and restore prior context on close; final per-screen completeness is verified independently of the helper.

`DisplayTransform` owns physical/logical dimensions and inverse touch rotation. The app draws to a logical portrait target and rotates once. Reflow/fit changes must preserve logical position. Fallback book artwork remains original native vectors, deterministic palette and stable portrait dimensions; cover loading never changes card size.

Validate actual screenshots for library, context, groups/shelves, metadata, delete confirmation, settings/focus, dark/sepia EPUB/PDF, landscape and both portraits. A screenshot proves drawing, not finger comfort or display latency; hardware remains UNVERIFIED until retested.
