#pragma once
#include <mupdf/fitz.h>
#include "papernx/mupdf_bridge.h"
fz_document *px_open_comic(fz_context *ctx, const char *path);
int px_comic_cover_page(fz_document *document);
int px_comic_toc(fz_context *ctx, fz_document *document, PxTocEntry *entries, int capacity);
int px_comic_page_size(fz_context *ctx, fz_document *document, int page, float *width, float *height);
// Reads cached dimensions without opening the archive or decoding on the UI thread.
int px_comic_known_size(fz_document *document,int page,float *width,float *height);
int px_comic_page_is_spread(fz_document *document, int page);
int px_comic_remember_size(fz_document *document, int page, float width, float height);
void px_comic_get_metrics(fz_document *document, PxComicMetrics *metrics);
