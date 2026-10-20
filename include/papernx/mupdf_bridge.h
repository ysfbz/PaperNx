#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PxDocument PxDocument;

typedef struct PxImage {
    int width;
    int height;
    int stride;
    unsigned char *rgb; // 24-bit RGB packed
} PxImage;

typedef struct PxTocEntry {
    char title[256];
    int page;
    int level;
    float y; // Link position within an unpaginated chapter; zero for comics.
} PxTocEntry;

// Lifecycle
PxDocument *px_document_open(const char *path, int width, int height, float em, const char *css, char *error, int capacity);
PxDocument *px_document_open_limited(const char *path, int width, int height, float em, const char *css, char *error, int capacity, size_t memory_limit);
int px_document_pages(PxDocument *doc);
int px_document_page_size(PxDocument *doc, int page, float *width, float *height);
int px_document_is_epub(PxDocument *doc);
int px_document_is_comic(PxDocument *doc);
// True only for an embedded EPUB cover; PDF thumbnails render page one instead.
int px_document_has_embedded_cover(PxDocument *doc);
int px_document_relayout(PxDocument *doc, int width, int height, float em, const char *css, char *error, int capacity);
int px_document_metadata(PxDocument *doc, const char *key, char *out, int capacity);
void px_document_close(PxDocument *doc);

// Rendering
int px_document_render(PxDocument *doc, int page, int max_width, int max_height, PxImage *image, char *error, int capacity);
int px_document_render_view(PxDocument *doc, int page, float zoom, int view_x, int view_y, int view_w, int view_h, PxImage *image, char *error, int capacity);
int px_document_render_thumbnail(PxDocument *doc, int max_w, int max_h, PxImage *image, char *error, int capacity);
// PDF pixel paper/text tint using the SAME resolved ReaderPreferences palette as EPUB.
// Neutral grayscale paper/ink pixels are mapped to exact background/foreground colors;
// chromatic document artwork is retained; original PDF geometry is untouched.
void px_image_apply_reading_colors(PxImage *image, uint32_t background_rgb, uint32_t foreground_rgb);
void px_image_filter(PxImage *image, int mode); // Legacy API, no longer used by the reader UI.
void px_image_free(PxImage *image);

// Versioned chapter/fraction locations; approximate after layout changes.
int64_t px_document_make_bookmark(PxDocument *doc, int page);
int px_document_lookup_bookmark(PxDocument *doc, int64_t bookmark);
// Persistent character + within-line position. Legacy page marks resolve with y=0.
// Geometry fallback is quantized to 1/8191 of a chapter; text to 1/1023 of a line.
int64_t px_document_make_position_bookmark(PxDocument *doc, int page, float y);
int px_document_lookup_position_bookmark(PxDocument *doc, int64_t bookmark, int *page, float *y);
int px_document_get_location(PxDocument *doc, int page, int *chapter, int *p);
int px_document_from_location(PxDocument *doc, int chapter, int p);

// Table of Contents (Outline)
int px_document_toc(PxDocument *doc, PxTocEntry *entries, int max_entries);

// Unpaginated chapter strips and persistent logical positions.
int px_document_render_tile(PxDocument *, int page, int y, int width, int height, PxImage *, char *, int);
int px_document_anchor(PxDocument *, int page, float y, int *chapter, int *character, double *offset);
int px_document_restore_anchor(PxDocument *, int chapter, int character, double offset, int *page, float *y);
#ifdef __cplusplus
}
#endif
