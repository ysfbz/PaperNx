# PaperNX interaction specification — updated for v0.9.1

This technical design originated in v0.4 and has been hardened for v0.9.1. Automated tests are reported in [TEST_RESULTS.md](TEST_RESULTS.md); **physical v0.9.1 acceptance remains UNVERIFIED**, as tracked in [DEVICE_TEST_PLAN.md](DEVICE_TEST_PLAN.md).

Reader D-pad Left/Right turns paginated EPUB, PDF, CBZ and CBR comic pages, including zoomed PDF/CBZ/CBR views. Crucially, in reader mode, D-pad Left/Right performs direct logical page turns without being transposed by display rotation, preserving natural reading ergonomics in portrait modes. Continuous EPUB Up/Down scrolls; Left/Right performs chapter/page-equivalent navigation. PDF/CBZ/CBR Up/Down pans vertically when zoomed; left and right analog sticks pan viewports smoothly; ZL/ZR adjusts zoom levels. Shoulder buttons (L/R) provide alternative page turn shortcuts. Center tap toggles reader tools; the toolbar and + menu expose settings, navigation, and library return.

Settings navigation and editing are distinct. Directional navigation moves the visible focus ring without modifying numeric values. A enters numeric editing, Left/Right adjusts, A commits, and B restores the captured value. Binary actions activate on A. Touch steppers use independent minus and plus hit areas. B closes the frontmost modal before navigating the underlying screen.

PDF, CBZ and CBR release gestures distinguish between panning and page turns: slow dragging pans the zoomed viewport, while a deliberate flick triggers page navigation. Multi-touch pinch-to-zoom and UI interactions never cause inadvertent page turns.

Library long press opens book actions; drag cancels hold, X opens the focused book context. Open/continue, favorite, reading status, hide/restore, metadata/details, collections and permanent deletion are separate actions. "Remove from Library" hides the item without touching the file. "Delete File Permanently" opens a two-step confirmation dialog showing the source path; cancelling never deletes.

Application → Display Orientation persists Landscape (default), Portrait clockwise, or Portrait counterclockwise. `DisplayTransform` shares logical dimensions and inverse touch mapping. Menu navigation follows the visual screen orientation, while reader page turns remain direct.

Appearance → Paper & Colors provides nine presets and visual background/text swatches with live preview and confirm/cancel. Advanced HEX is optional. Paper textures apply to the EPUB canvas; PDF and comic (CBZ/CBR) illustrations retain their original RGB appearance without destructive paper or ink filtering.

Pressed feedback begins on finger down; safe actions commit on release. Modal touch routing precedes book gestures. Touch and controller input coexist seamlessly.

Current v0.9.1 paginated modes additionally use `PageCurlRenderer` with interactive commit/cancel on touch and an on/off Navigation preference; the reader has a three-tab page/chapter/bookmark navigator and background preview worker. Large-image UI/flip latency on physical Switch remains UNVERIFIED; the historical v0.4 interaction specification is not a substitute for the 38-item physical device matrix.
