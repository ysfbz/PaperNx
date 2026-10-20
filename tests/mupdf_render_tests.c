#include "papernx/mupdf_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

static uint64_t fnv1a_hash(const unsigned char *data, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= (uint64_t)data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void save_ppm(const char *path, const PxImage *img) {
    if (!img || !img->rgb || img->width <= 0 || img->height <= 0) return;
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", img->width, img->height);
    fwrite(img->rgb, 1, (size_t)img->width * img->height * 3, f);
    fclose(f);
    printf("Saved render snapshot to %s (%dx%d, RGB24)\n", path, img->width, img->height);
}

int main(int argc, char **argv) {
    const char *epub_path = "tests/fixtures/sample_book.epub";
    const char *pdf_path = "tests/fixtures/sample_doc.pdf";
    const char *corrupt_epub = "tests/fixtures/corrupt.epub";
    const char *corrupt_pdf = "tests/fixtures/corrupt.pdf";

    if (argc >= 3) {
        epub_path = argv[1];
        pdf_path = argv[2];
    }

    printf("==========================================================\n");
    printf("PaperNX — Native MuPDF Rendering & Integration Test Suite\n");
    printf("==========================================================\n");

    // 1. Error Resiliency: Corrupt EPUB & PDF
    {
        char err[256] = {0};
        PxDocument *bad_epub = px_document_open(corrupt_epub, 1280, 720, 22.0f, "", err, sizeof(err));
        assert(bad_epub == NULL);
        assert(strlen(err) > 0);
        printf("[PASS] Corrupted EPUB rejected safely without crash: %s\n", err);

        err[0] = '\0';
        PxDocument *bad_pdf = px_document_open(corrupt_pdf, 1280, 720, 22.0f, "", err, sizeof(err));
        assert(bad_pdf == NULL);
        assert(strlen(err) > 0);
        printf("[PASS] Corrupted PDF rejected safely without crash: %s\n", err);
    }

    // Real OPF metadata: namespace prefixes, EPUB3 refinements, bounded UTF-8,
    // missing optional fields and malformed numeric values.
    {
        char err[256], value[1200];
        PxDocument *doc = px_document_open("tests/fixtures/metadata_epub3.epub", 720, 1280, 24, "", err, sizeof err);
        assert(doc);
        px_document_metadata(doc, "info:Subject", value, sizeof value);
        assert(strcmp(value, "\xc5\x9e" "iir") == 0);
        px_document_metadata(doc, "info:Series", value, sizeof value);
        assert(strcmp(value, "Okuma Dizisi") == 0);
        px_document_metadata(doc, "info:SeriesIndex", value, sizeof value);
        assert(strcmp(value, "3") == 0);
        struct { char value[2]; unsigned char guard; } tiny = {{'x', 'x'}, 0xa5};
        px_document_metadata(doc, "info:Author", tiny.value, sizeof tiny.value);
        assert(tiny.value[0] == 0 && tiny.guard == 0xa5); // U+0130 requires two bytes plus NUL.
        assert(px_document_metadata(doc, "info:Author", value, 1) > 1 && value[0] == 0);
        px_document_close(doc);
        doc = px_document_open("tests/fixtures/metadata_bounded.epub", 720, 1280, 24, "", err, sizeof err);
        assert(doc);
        px_document_metadata(doc, "info:Subject", value, sizeof value);
        size_t length = strlen(value);
        assert(length > 0 && length < 1024 && length % 2 == 0);
        for (size_t i = 0; i < length; i += 2) assert((unsigned char)value[i] == 0xc4 && (unsigned char)value[i+1] == 0xb1);
        px_document_metadata(doc, "info:SeriesIndex", value, sizeof value);
        assert(value[0] == 0);
        px_document_close(doc);
        doc = px_document_open("tests/fixtures/metadata_missing.epub", 720, 1280, 24, "", err, sizeof err);
        assert(doc);
        px_document_metadata(doc, "info:Subject", value, sizeof value);
        assert(value[0] == 0);
        px_document_metadata(doc, "info:Series", value, sizeof value);
        assert(value[0] == 0);
        px_document_close(doc);
        printf("[PASS] Bounded embedded metadata and EPUB3 series refinements\n");
    }

    // Native CBZ path: reading directly from ZIP, original RGB image data,
    // bounded document allocation and clean rejection of truncated archives.
    {
        char err[256] = {0};
        PxDocument *comic = px_document_open_limited(
            "tests/fixtures/generated/two-pages.cbz", 320, 480, 18.0f,
            "", err, sizeof(err), 16u * 1024u * 1024u);
        assert(comic != NULL);
        assert(!px_document_is_epub(comic));
        assert(px_document_pages(comic) == 2);
        PxImage image = {0};
        assert(px_document_render(comic, 0, 320, 480, &image, err, sizeof(err)));
        assert(image.rgb && image.width > 0 && image.height > 0);
        px_image_free(&image);
        px_document_close(comic);
        PxDocument *corrupt = px_document_open_limited(
            "tests/fixtures/generated/corrupt.cbz", 320, 480, 18.0f,
            "", err, sizeof(err), 16u * 1024u * 1024u);
        assert(corrupt == NULL);
        printf("[PASS] CBZ ZIP document with two PNG pages; damaged ZIP rejected\n");
    }

    // 2. Real EPUB Document Verification
    {
        char err[256] = {0};
        PxDocument *epub = px_document_open(epub_path, 1280, 720, 24.0f, "", err, sizeof(err));
        if (!epub) {
            fprintf(stderr, "FATAL: Failed to open %s: %s\n", epub_path, err);
            return 1;
        }

        assert(px_document_is_epub(epub) == 1);
        assert(px_document_has_embedded_cover(epub) == 1);
        int page_count = px_document_pages(epub);
        printf("EPUB opened successfully. Page count: %d\n", page_count);
        assert(page_count >= 1);

        char title[128] = {0};
        char author[128] = {0};
        px_document_metadata(epub, "info:Title", title, sizeof(title));
        px_document_metadata(epub, "info:Author", author, sizeof(author));
        printf("EPUB Title: '%s', Author: '%s'\n", title, author);
        assert(strstr(author, "; Yusuf ") != NULL);
        char subject[1024], series[512], index[64];
        assert(px_document_metadata(epub, "info:Subject", subject, sizeof subject) > 0);
        assert(strcmp(subject, "Roman; T\xc3\xbcrk Edebiyat\xc4\xb1") == 0);
        assert(px_document_metadata(epub, "info:Series", series, sizeof series) > 0);
        assert(strcmp(series, "Bo\xc4\x9f" "azi\xc3\xa7i") == 0);
        px_document_metadata(epub, "info:SeriesIndex", index, sizeof index);
        assert(strcmp(index, "2.5") == 0);
        struct { unsigned char before; char value[2]; unsigned char after; } tiny = {0x55, {0}, 0xaa};
        px_document_metadata(epub, "info:Series", tiny.value, sizeof tiny.value);
        assert(tiny.before == 0x55 && tiny.after == 0xaa && strcmp(tiny.value, "B") == 0);
        assert(strstr(title, "İstanbul") != NULL);
        assert(strstr(author, "Tanpınar") != NULL);

        // Render Page 0
        PxImage img0 = {0};
        int r0 = px_document_render(epub, 0, 1280, 720, &img0, err, sizeof(err));
        assert(r0 != 0);
        assert(img0.rgb != NULL);
        assert(img0.width > 0 && img0.height > 0);
        uint64_t hash_epub0 = fnv1a_hash(img0.rgb, (size_t)img0.width * img0.height * 3);
        printf("[PASS] EPUB Page 0 rendered: %dx%d, hash: 0x%016llx\n", img0.width, img0.height, (unsigned long long)hash_epub0);
        save_ppm("tests/fixtures/rendered_epub_page0.ppm", &img0);
        px_image_free(&img0);

        // Relayout with different font size & verify reflow changes hash
        int relayout_ok = px_document_relayout(epub, 1280, 720, 36.0f, "", err, sizeof(err));
        assert(relayout_ok != 0);
        PxImage img_reflow = {0};
        int r_reflow = px_document_render(epub, 0, 1280, 720, &img_reflow, err, sizeof(err));
        assert(r_reflow != 0);
        uint64_t hash_reflow = fnv1a_hash(img_reflow.rgb, (size_t)img_reflow.width * img_reflow.height * 3);
        printf("[PASS] EPUB Page 0 reflowed (larger font): %dx%d, hash: 0x%016llx\n", img_reflow.width, img_reflow.height, (unsigned long long)hash_reflow);
        assert(hash_epub0 != hash_reflow);
        px_image_free(&img_reflow);

        // Test TOC outline
        PxTocEntry toc[16];
        int toc_count = px_document_toc(epub, toc, 16);
        printf("EPUB TOC count: %d\n", toc_count);
        assert(toc_count >= 2);
        printf("TOC[0]: '%s' -> page %d\n", toc[0].title, toc[0].page);
        printf("TOC[1]: '%s' -> page %d\n", toc[1].title, toc[1].page);
        assert(strstr(toc[0].title, "Birinci Bölüm") != NULL);
        assert(strstr(toc[1].title, "İkinci Bölüm") != NULL);

        // Test Bookmark creation and lookup
        int64_t bm = px_document_make_bookmark(epub, 0);
        assert(bm != -1);
        int looked_up = px_document_lookup_bookmark(epub, bm);
        printf("Bookmark for page 0 resolved to page: %d\n", looked_up);
        assert(looked_up == 0);

        // Test Chapter / Location conversion
        int ch = -1, locP = -1;
        int loc_ok = px_document_get_location(epub, 0, &ch, &locP);
        printf("Location for page 0: ok=%d, chapter=%d, p=%d\n", loc_ok, ch, locP);
        if (loc_ok) {
            int backPage = px_document_from_location(epub, ch, locP);
            printf("Resolved from location (chapter=%d, p=%d): page %d\n", ch, locP, backPage);
            assert(backPage == 0);
        }

        px_document_close(epub);
        printf("[PASS] EPUB tests completed successfully!\n");
    }

    // 3. Real Multi-Page PDF Document Verification
    {
        char err[256] = {0};
        PxDocument *pdf = px_document_open(pdf_path, 1280, 720, 22.0f, "", err, sizeof(err));
        if (!pdf) {
            fprintf(stderr, "FATAL: Failed to open %s: %s\n", pdf_path, err);
            return 1;
        }

        assert(px_document_is_epub(pdf) == 0);
        char pdf_subject[128], pdf_author[128];
        px_document_metadata(pdf, "info:Subject", pdf_subject, sizeof pdf_subject);
        px_document_metadata(pdf, "info:Author", pdf_author, sizeof pdf_author);
        assert(strcmp(pdf_subject, "PDF Genre") == 0 && strcmp(pdf_author, "PDF Author") == 0);
        assert(px_document_has_embedded_cover(pdf) == 0);
        assert(px_document_has_embedded_cover(NULL) == 0);
        int page_count = px_document_pages(pdf);
        printf("PDF opened successfully. Page count: %d\n", page_count);
        assert(page_count == 2);

        // Render Page 0 (Fit Page)
        PxImage pdf_img0 = {0};
        int r0 = px_document_render(pdf, 0, 1280, 720, &pdf_img0, err, sizeof(err));
        assert(r0 != 0);
        assert(pdf_img0.rgb != NULL);
        uint64_t hash_pdf0 = fnv1a_hash(pdf_img0.rgb, (size_t)pdf_img0.width * pdf_img0.height * 3);
        printf("[PASS] PDF Page 0 rendered: %dx%d, hash: 0x%016llx\n", pdf_img0.width, pdf_img0.height, (unsigned long long)hash_pdf0);
        save_ppm("tests/fixtures/rendered_pdf_page0.ppm", &pdf_img0);
        px_image_free(&pdf_img0);

        // Render Page 1 (Must differ from Page 0)
        PxImage pdf_img1 = {0};
        int r1 = px_document_render(pdf, 1, 1280, 720, &pdf_img1, err, sizeof(err));
        assert(r1 != 0);
        uint64_t hash_pdf1 = fnv1a_hash(pdf_img1.rgb, (size_t)pdf_img1.width * pdf_img1.height * 3);
        printf("[PASS] PDF Page 1 rendered: %dx%d, hash: 0x%016llx\n", pdf_img1.width, pdf_img1.height, (unsigned long long)hash_pdf1);
        assert(hash_pdf0 != hash_pdf1);
        px_image_free(&pdf_img1);

        // Render Zoomed & Panned Page 0 (1.5x zoom, panX = 100, panY = 50)
        PxImage pdf_zoom = {0};
        int rz = px_document_render_view(pdf, 0, 1.5f, 100, 50, 1280, 720, &pdf_zoom, err, sizeof(err));
        assert(rz != 0);
        uint64_t hash_zoom = fnv1a_hash(pdf_zoom.rgb, (size_t)pdf_zoom.width * pdf_zoom.height * 3);
        printf("[PASS] PDF Page 0 Zoomed 1.5x: %dx%d, hash: 0x%016llx\n", pdf_zoom.width, pdf_zoom.height, (unsigned long long)hash_zoom);
        assert(hash_pdf0 != hash_zoom);
        px_image_free(&pdf_zoom);

        // The same EPUB paper/text RGB mapping is applied to neutral PDF
        // raster pixels, while colored illustrations retain their colors.
        unsigned char sample[12]={255,255,255,0,0,0,128,128,128,255,0,0};
        PxImage mapped={2,2,6,sample};
        px_image_apply_reading_colors(&mapped,0xece0c6u,0x413224u);
        assert(sample[0]==0xec && sample[1]==0xe0 && sample[2]==0xc6);
        assert(sample[3]==0x41 && sample[4]==0x32 && sample[5]==0x24);
        assert(sample[9]==255 && sample[10]==0 && sample[11]==0);
        px_image_apply_reading_colors(NULL,0,0);
        // Viewport allocation stays bounded even at maximum zoom/extreme pan.
        PxImage bounded = {0};
        assert(px_document_render_view(pdf, 0, 5, 1000000, 1000000, 640, 480, &bounded, err, sizeof err));
        assert(bounded.width == 640 && bounded.height == 480 && bounded.stride == 640 * 3);
        const uint64_t original = fnv1a_hash(bounded.rgb, (size_t)bounded.stride * bounded.height);
        px_image_filter(&bounded, 0);
        assert(original == fnv1a_hash(bounded.rgb, (size_t)bounded.stride * bounded.height));
        px_image_filter(&bounded, 3); px_image_filter(&bounded, 3);
        assert(original == fnv1a_hash(bounded.rgb, (size_t)bounded.stride * bounded.height));
        px_image_free(&bounded);
        uint64_t filters[3];
        for (int mode = 0; mode < 3; ++mode) {
            assert(px_document_render_view(pdf, 0, 1, 0, 0, 640, 480, &bounded, err, sizeof err));
            px_image_filter(&bounded, mode);
            filters[mode] = fnv1a_hash(bounded.rgb, (size_t)bounded.stride * bounded.height);
            px_image_free(&bounded);
        }
        assert(filters[0] != filters[1] && filters[0] != filters[2] && filters[1] != filters[2]);
        assert(px_document_render_view(pdf, 0, .5f, 0, 0, 640, 480, &bounded, err, sizeof err));
        px_image_free(&bounded);
        // PDF TOC
        PxTocEntry pdf_toc[8];
        int pdf_toc_count = px_document_toc(pdf, pdf_toc, 8);
        printf("PDF TOC count: %d\n", pdf_toc_count);
        assert(pdf_toc_count == 2);
        printf("PDF TOC[0]: '%s' -> page %d\n", pdf_toc[0].title, pdf_toc[0].page);
        printf("PDF TOC[1]: '%s' -> page %d\n", pdf_toc[1].title, pdf_toc[1].page);
        assert(strstr(pdf_toc[0].title, "Sayfa 1") != NULL);
        assert(strstr(pdf_toc[1].title, "Sayfa 2") != NULL);
        assert(pdf_toc[0].page == 0);
        assert(pdf_toc[1].page == 1);

        px_document_close(pdf);
        printf("[PASS] PDF tests completed successfully!\n");
    }

    printf("==========================================================\n");
    printf("ALL NATIVE MUPDF EPUB & PDF TESTS PASSED WITH PROVEN RENDERING!\n");
    printf("==========================================================\n");
    return 0;
}
