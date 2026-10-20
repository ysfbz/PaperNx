# PaperNX Touch and Controller Controls

Controls overview for touch gestures and Nintendo Switch controllers (Joy-Con and Pro Controller).

| Action | Touch | Controller |
|---|---|---|
| Quick actions | Top-right + | Plus (+) |
| Open book | Tap library card/file | A on focused item |
| Book actions | Hold card; drag cancels | X on focused book |
| Paginated EPUB / PDF / CBZ / CBR page | Left/right zones or intentional horizontal flick | D-pad Left/Right; L / R shoulder buttons |
| Continuous EPUB | Vertical drag | Up/Down scroll; Left/Right chapter/page-equivalent navigation |
| PDF / CBZ / CBR pan | Slow drag while zoomed | Left analog stick; Right analog stick |
| PDF / CBZ / CBR zoom | Toolbar minus/plus or pinch-to-zoom | ZL / ZR |
| Reader tools | Center tap | A |
| Settings | + → Reading settings | Reader X; focused action A |
| Numeric setting | Independent minus/plus | A edit; Left/Right adjust; A confirm; B cancel/restore |
| Palette/swatches | Tap preset or swatch, preview/confirm | Move focus, A select/confirm, B cancel |
| TOC/bookmarks | + → named action; select row | Focus/A; B back |
| Return to library | + → Return to library | B after frontmost modal closes |
| Portrait orientation | Settings → Application → Display Orientation | Focus setting and activate |
| Exit application | + → Exit | Root back/exit route |

### Comic and page-navigation behavior
- **Page curl:** In paginated mode, a horizontal page-turn drag follows the finger and either commits or cancels on release; the animation can be disabled in Reader Settings → Navigation. Zoomed comics reserve page-edge gestures for turning while interior dragging pans the image. Continuous-reading mode scrolls rather than curls.
- **Comic navigation:** The reader offers Chapters, All Pages (thumbnail grid) and Bookmarks in the contents modal, plus a draggable progress scrubber/preview. ComicInfo series/issue metadata may group books on a series shelf.
- **Comic reading appearance:** Original artwork RGB is preserved by default; selected book/global theme colors fill the surrounding viewport. An optional artwork reading palette and manga right-to-left page order are separately selectable; Hebrew/BiDi language shaping is not included.
- **Zoom reset:** A visible Fit page affordance appears while applicable when zoomed; triggers ZL/ZR adjust zoom in the reader.

### Controller & Gesture Behaviors
- **Reader Page Turns:** In reader mode, D-pad Left/Right and L/R shoulder buttons directly turn pages without being redirected by display rotation, ensuring consistent reading ergonomics across landscape and portrait orientations.
- **Menu & UI Directional Navigation:** Menu focus, settings navigation, and library grids follow the active visual screen orientation.
- **Distinction between Selecting and Editing:** Focus navigation moves the visible focus ring without mutating values. Pressing A enters edit mode; Left/Right adjusts; A commits; B restores the prior value.
- **Modals:** A frontmost modal captures all input and blocks document page turns underneath.
- **PDF, CBZ & CBR Gestures:** A quick flick changes page; a slow drag pans the zoomed viewport. Pinch-to-zoom and toolbar interactions do not trigger page turns.
- **Library File Actions:** "Hide/Remove from Library" removes the item from the library view while leaving the source file untouched on SD card. "Delete File Permanently" requires explicit two-step confirmation.
- **Display Orientation:** Landscape is default. Clockwise and counterclockwise portrait modes rotate display output and inversely map touchscreen coordinates. Accelerometer auto-rotation is not supported.

Automated interaction suites pass across all viewports. Physical Switch hardware checks remain **UNVERIFIED** pending real-device testing.
