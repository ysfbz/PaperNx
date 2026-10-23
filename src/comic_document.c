/* Comic archive reader. No archive entry is extracted to the filesystem.
 * ZIP/RAR5 use libarchive; RAR4 uses the LGPL unarr decoder, including solid archives.
 * Images and indexes are owned by the same bounded MuPDF context as the document. */
#include "papernx/comic_document.h"
#include <archive.h>
#include <archive_entry.h>
#include <webp/decode.h>
#include "unarr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>

/* libarchive converts ZIP filenames through LC_CTYPE. Initialise it once before
 * any archive reader is created; never change numeric/date formatting. */
static pthread_once_t comic_locale_once=PTHREAD_ONCE_INIT;
static void comic_utf8_locale(void) {
    if(!setlocale(LC_CTYPE,"C.UTF-8"))
        if(!setlocale(LC_CTYPE,"en_US.UTF-8"))setlocale(LC_CTYPE,"UTF-8");
}

#define COMIC_MAX_ENTRIES 50000
#define COMIC_MAX_PAGES 20000
#define COMIC_MAX_IMAGE (48u*1024u*1024u)
#define COMIC_MAX_XML (1024u*1024u)

typedef struct {
    char *name;
    int ordinal;
    off64_t offset;
    size_t bytes;
    float width,height;
    int double_page;
} ComicEntry;
typedef struct {
    fz_document super;
    char *path;
    ComicEntry *entries;
    int count, capacity, rar4, cover, manga;
    int cached_page;
    fz_image *cached_image;
    ar_stream *stream;
    ar_archive *rar;
    struct archive *cursor;
    int cursor_ordinal;
    char title[512], author[1024], genre[512], series[512], number[64];
    PxTocEntry outline[256];
    int outline_count;
} ComicDocument;
typedef struct { fz_page super; fz_image *image; } ComicPage;

void ar_set_custom_allocator(void *(*allocate)(void *,size_t),void (*release)(void *,void *),void *opaque);
static void *comic_allocate(void *ctx,size_t bytes) {return fz_malloc_no_throw(ctx,bytes);}
static void comic_release(void *ctx,void *p) {fz_free(ctx,p);}
static void use_allocator(fz_context *ctx) {ar_set_custom_allocator(comic_allocate,comic_release,ctx);}
static void reset_allocator(void) {ar_set_custom_allocator(NULL,NULL,NULL);}

static void copy_text(char *out,size_t cap,const char *src) {
    size_t n=strlen(src);if(n>=cap)n=cap-1;
    while(n && ((unsigned char)src[n]&0xc0)==0x80)--n;
    memcpy(out,src,n);out[n]=0;
}
static int valid_name(const char *name) {
    if(!name || !*name || strlen(name)>2048 || *name=='/' || *name=='\\' || strchr(name,':'))return 0;
    for(const char *p=name;*p;) {
        const char *end=p;while(*end && *end!='/' && *end!='\\')++end;
        size_t n=(size_t)(end-p);
        if(!n || *p=='.' || (n==8 && !fz_strncasecmp(p,"__MACOSX",8)))return 0;
        p=*end?end+1:end;
    }
    return 1;
}
static int image_name(const char *name) {
    const char *ext=strrchr(name,'.');
    static const char *extensions[]={".jpg",".jpeg",".png",".webp",".gif",".bmp",".tif",".tiff",".jp2",".jpx",NULL};
    if(!ext)return 0;
    for(int i=0;extensions[i];++i)if(!fz_strcasecmp(ext,extensions[i]))return 1;
    return 0;
}
static int is_info(const char *name) {
    const char *base=name;for(const char *p=name;*p;++p)if(*p=='/' || *p=='\\')base=p+1;
    return !fz_strcasecmp(base,"ComicInfo.xml");
}
static int natural_names(const void *left,const void *right) {
    const ComicEntry *a=left,*b=right;
    const unsigned char *x=(const unsigned char *)a->name,*y=(const unsigned char *)b->name;
    while(*x && *y) {
        if(*x>='0' && *x<='9' && *y>='0' && *y<='9') {
            const unsigned char *xe=x,*ye=y;
            while(*xe>='0' && *xe<='9')++xe;
            while(*ye>='0' && *ye<='9')++ye;
            while(x<xe && *x=='0')++x;
            while(y<ye && *y=='0')++y;
            if(xe-x!=ye-y)return xe-x<ye-y?-1:1;
            int result=memcmp(x,y,(size_t)(xe-x));if(result)return result;
            x=xe;y=ye;
        } else {
            unsigned char cx=*x++,cy=*y++;
            if(cx>='A' && cx<='Z')cx+='a'-'A';
            if(cy>='A' && cy<='Z')cy+='a'-'A';
            if(cx!=cy)return cx<cy?-1:1;
        }
    }
    if(*x!=*y)return *x?1:-1;
    int result=strcmp(a->name,b->name);
    return result?result:(a->ordinal>b->ordinal)-(a->ordinal<b->ordinal);
}
static void add_entry(fz_context *ctx,ComicDocument *doc,const char *name,int ordinal,off64_t offset,size_t size) {
    if(doc->count>=COMIC_MAX_PAGES)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic exceeds 20000 pages");
    if(doc->count==doc->capacity) {
        doc->capacity=doc->capacity?doc->capacity*2:64;
        doc->entries=fz_realloc_array(ctx,doc->entries,doc->capacity,ComicEntry);
    }
    ComicEntry *entry=&doc->entries[doc->count];
    entry->name=fz_strdup(ctx,name);
    for(char *p=entry->name;*p;++p)if(*p=='\\')*p='/';
    entry->ordinal=ordinal;entry->offset=offset;entry->bytes=size;entry->width=entry->height=0;entry->double_page=0;++doc->count;
}
static struct archive *open_archive(fz_context *ctx,const char *path) {
    struct archive *reader=archive_read_new();
    if(!reader)fz_throw(ctx,FZ_ERROR_MEMORY,"Cannot allocate comic archive reader");
    archive_read_support_filter_none(reader);
    archive_read_support_format_zip(reader);archive_read_support_format_rar(reader);archive_read_support_format_rar5(reader);
    if(archive_read_open_filename(reader,path,65536)!=ARCHIVE_OK) {
        char message[256];snprintf(message,sizeof message,"Cannot open comic archive: %s",archive_error_string(reader));
        archive_read_free(reader);fz_throw(ctx,FZ_ERROR_GENERIC,"%s",message);
    }
    return reader;
}
static fz_buffer *read_archive_entry(fz_context *ctx,struct archive *reader,size_t limit) {
    fz_buffer *buffer=NULL;fz_var(buffer);
    fz_try(ctx) {
        buffer=fz_new_buffer(ctx,4096);unsigned char chunk[8192];size_t total=0;la_ssize_t n;
        while((n=archive_read_data(reader,chunk,sizeof chunk))>0) {
            if((size_t)n>limit-total)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic entry exceeds memory budget");
            fz_append_data(ctx,buffer,chunk,(size_t)n);total+=(size_t)n;
        }
        if(n<0)fz_throw(ctx,FZ_ERROR_GENERIC,"Cannot decode comic entry: %s",archive_error_string(reader));
    }
    fz_catch(ctx) {fz_drop_buffer(ctx,buffer);fz_rethrow(ctx);}
    return buffer;
}
static fz_buffer *read_rar_entry(fz_context *ctx,ComicDocument *doc,off64_t offset,size_t limit) {
    fz_buffer *buffer=NULL;fz_var(buffer);use_allocator(ctx);
    fz_try(ctx) {
        if(!ar_parse_entry_at(doc->rar,offset))fz_throw(ctx,FZ_ERROR_GENERIC,"Cannot seek comic RAR page");
        const size_t size=ar_entry_get_size(doc->rar);
        if(size>limit)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic entry exceeds memory budget");
        buffer=fz_new_buffer(ctx,size);unsigned char chunk[8192];size_t remaining=size;
        while(remaining) {
            size_t n=remaining<sizeof chunk?remaining:sizeof chunk;
            if(!ar_entry_uncompress(doc->rar,chunk,n))fz_throw(ctx,FZ_ERROR_GENERIC,"Cannot decode RAR page (damaged, encrypted or incomplete archive)");
            fz_append_data(ctx,buffer,chunk,n);remaining-=n;
        }
    }
    fz_always(ctx) {reset_allocator();}
    fz_catch(ctx) {fz_drop_buffer(ctx,buffer);fz_rethrow(ctx);}
    return buffer;
}
static void node_text(fz_xml *node,char *out,size_t cap,int depth) {
    if(depth>16)return;
    for(fz_xml *p=fz_xml_down(node);p;p=fz_xml_next(p)) {
        const char *text=fz_xml_text(p);
        if(text)fz_strlcat(out,text,cap);else node_text(p,out,cap,depth+1);
    }
}
static void field_text(fz_xml *root,const char *tag,char *out,size_t cap) {
    fz_xml *field=fz_xml_find_down(root,tag);
    if(field) {char raw[2048]="";node_text(field,raw,sizeof raw,0);copy_text(out,cap,raw);}
}
static int safe_index(const char *text,int count) {
    if(!text || !*text)return -1;
    unsigned int value=0;
    for(const unsigned char *p=(const unsigned char *)text;*p;++p) {
        if(*p<'0' || *p>'9' || value>20000)return -1;
        value=value*10+*p-'0';
    }
    return value<(unsigned)count?(int)value:-1;
}
static void load_info(fz_context *ctx,ComicDocument *doc,fz_buffer *info) {
    fz_xml *xml=NULL;fz_var(xml);
    fz_try(ctx) {
        xml=fz_parse_xml(ctx,info,0);
        fz_xml *root=fz_xml_find_dfs(xml,"ComicInfo",NULL,NULL);
        if(root) {
            field_text(root,"Title",doc->title,sizeof doc->title);field_text(root,"Writer",doc->author,sizeof doc->author);
            if(!*doc->author)field_text(root,"Penciller",doc->author,sizeof doc->author);
            field_text(root,"Genre",doc->genre,sizeof doc->genre);field_text(root,"Series",doc->series,sizeof doc->series);
            field_text(root,"Number",doc->number,sizeof doc->number);
            char manga[64]="";field_text(root,"Manga",manga,sizeof manga);
            doc->manga=!fz_strcasecmp(manga,"YesAndRightToLeft");
            fz_xml *pages=fz_xml_find_down(root,"Pages");
            for(fz_xml *page=pages?fz_xml_down(pages):NULL;page;page=fz_xml_next(page)) {
                int index=safe_index(fz_xml_att(page,"Image"),doc->count);
                if(index<0)continue;
                const char *type=fz_xml_att(page,"Type"),*bookmark=fz_xml_att(page,"Bookmark");
                if(type && !fz_strcasecmp(type,"FrontCover"))doc->cover=index;
                const char *spread=fz_xml_att(page,"DoublePage");
                doc->entries[index].double_page=spread && (!fz_strcasecmp(spread,"True") || !strcmp(spread,"1"));
                if(bookmark && *bookmark && doc->outline_count<256) {
                    PxTocEntry *entry=&doc->outline[doc->outline_count++];
                    copy_text(entry->title,sizeof entry->title,bookmark);entry->page=index;entry->level=0;entry->y=0;
                }
            }
            if(!*doc->title && *doc->series) {
                char title[640];snprintf(title,sizeof title,"%s%s%s",doc->series,*doc->number?" #":"",doc->number);
                copy_text(doc->title,sizeof doc->title,title);
            }
        }
    }
    fz_always(ctx) {fz_drop_xml(ctx,xml);}
    fz_catch(ctx) {fz_rethrow_if(ctx,FZ_ERROR_MEMORY);fz_warn(ctx,"Ignoring damaged optional ComicInfo.xml");}
}
static void index_archive(fz_context *ctx,ComicDocument *doc) {
    struct archive *reader=NULL;fz_buffer *info=NULL;fz_var(reader);fz_var(info);
    fz_try(ctx) {
        reader=open_archive(ctx,doc->path);struct archive_entry *entry=NULL;int status,ordinal=0;
        while((status=archive_read_next_header(reader,&entry))==ARCHIVE_OK || status==ARCHIVE_WARN) {
            if(++ordinal>COMIC_MAX_ENTRIES)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic archive has too many entries");
            if(archive_entry_is_encrypted(entry)>0)fz_throw(ctx,FZ_ERROR_GENERIC,"Password-protected comics are not supported");
            const char *name=archive_entry_pathname_utf8(entry);
            if(!name)name=archive_entry_pathname(entry);
            const int kind=archive_entry_filetype(entry);
            if(valid_name(name) && (kind==AE_IFREG || kind==0)) {
                if(image_name(name)) {
                    la_int64_t size=archive_entry_size(entry);
                    if(size<0 || (uint64_t)size>COMIC_MAX_IMAGE)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic image exceeds 48 MiB limit");
                    add_entry(ctx,doc,name,ordinal,0,(size_t)size);
                } else if(!info && is_info(name)) info=read_archive_entry(ctx,reader,COMIC_MAX_XML);
            }
            if(archive_read_data_skip(reader)<ARCHIVE_OK)fz_throw(ctx,FZ_ERROR_GENERIC,"Damaged or incomplete comic archive: %s",archive_error_string(reader));
        }
        if(status!=ARCHIVE_EOF)fz_throw(ctx,FZ_ERROR_GENERIC,"Cannot index comic archive: %s",archive_error_string(reader));
        if(!doc->count)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic contains no supported page images");
        qsort(doc->entries,(size_t)doc->count,sizeof *doc->entries,natural_names);
        if(info)load_info(ctx,doc,info);
    }
    fz_always(ctx) {if(reader)archive_read_free(reader);fz_drop_buffer(ctx,info);}
    fz_catch(ctx) {fz_rethrow(ctx);}
}
static void index_rar4(fz_context *ctx,ComicDocument *doc) {
    fz_buffer *info=NULL;fz_var(info);use_allocator(ctx);
    fz_try(ctx) {
        doc->stream=ar_open_file(doc->path);
        if(!doc->stream || !(doc->rar=ar_open_rar_archive(doc->stream)))fz_throw(ctx,FZ_ERROR_GENERIC,"Cannot open RAR4 comic");
        int ordinal=0;off64_t infoOffset=0;
        while(ar_parse_entry(doc->rar)) {
            if(++ordinal>COMIC_MAX_ENTRIES)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic archive has too many entries");
            const char *name=ar_entry_get_name(doc->rar);
            if(!valid_name(name))continue;
            if(image_name(name)) {
                size_t size=ar_entry_get_size(doc->rar);
                if(size>COMIC_MAX_IMAGE)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic image exceeds 48 MiB limit");
                add_entry(ctx,doc,name,ordinal,ar_entry_get_offset(doc->rar),size);
            } else if(!infoOffset && is_info(name)) infoOffset=ar_entry_get_offset(doc->rar);
        }
        if(!ar_at_eof(doc->rar))fz_throw(ctx,FZ_ERROR_GENERIC,"Encrypted, split or damaged RAR4 comic archive");
        if(!doc->count)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic contains no supported page images");
        qsort(doc->entries,(size_t)doc->count,sizeof *doc->entries,natural_names);
        if(infoOffset) {info=read_rar_entry(ctx,doc,infoOffset,COMIC_MAX_XML);load_info(ctx,doc,info);}
    }
    fz_always(ctx) {reset_allocator();fz_drop_buffer(ctx,info);}
    fz_catch(ctx) {fz_rethrow(ctx);}
}
static fz_buffer *page_buffer(fz_context *ctx,ComicDocument *doc,int number) {
    ComicEntry *wanted=&doc->entries[number];
    if(doc->rar4)return read_rar_entry(ctx,doc,wanted->offset,COMIC_MAX_IMAGE);
    fz_buffer *buffer=NULL;fz_var(buffer);
    fz_try(ctx) {
        /* Preserve the RAR5 solid dictionary for physical forward reads.
         * Reopening the archive for each page would replay the whole prefix. */
        if(doc->cursor && wanted->ordinal<=doc->cursor_ordinal) {
            archive_read_free(doc->cursor);doc->cursor=NULL;doc->cursor_ordinal=0;
        }
        if(!doc->cursor){doc->cursor=open_archive(ctx,doc->path);doc->cursor_ordinal=0;}
        struct archive_entry *entry=NULL;int status;
        while((status=archive_read_next_header(doc->cursor,&entry))==ARCHIVE_OK || status==ARCHIVE_WARN) {
            if(++doc->cursor_ordinal==wanted->ordinal){buffer=read_archive_entry(ctx,doc->cursor,COMIC_MAX_IMAGE);break;}
            if(archive_read_data_skip(doc->cursor)<ARCHIVE_OK)fz_throw(ctx,FZ_ERROR_GENERIC,"Cannot skip damaged comic page");
        }
        if(!buffer)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic archive changed or a page is missing");
    }
    fz_catch(ctx) {
        if(doc->cursor)archive_read_free(doc->cursor);
        doc->cursor=NULL;doc->cursor_ordinal=0;
        fz_drop_buffer(ctx,buffer);fz_rethrow(ctx);
    }
    return buffer;
}
static fz_image *load_image(fz_context *ctx,ComicDocument *doc,int number) {
    fz_buffer *buffer=NULL;fz_image *image=NULL;fz_pixmap *pix=NULL;
    fz_var(buffer);fz_var(image);fz_var(pix);
    fz_try(ctx) {
        buffer=page_buffer(ctx,doc,number);
        const char *extension=strrchr(doc->entries[number].name,'.');
        if(extension && !fz_strcasecmp(extension,".webp")) {
            unsigned char *data=NULL;size_t size=fz_buffer_storage(ctx,buffer,&data);int w,h;
            if(!WebPGetInfo(data,size,&w,&h) || w<=0 || h<=0 || (uint64_t)w*h>64000000)
                fz_throw(ctx,FZ_ERROR_GENERIC,"Invalid or oversized WebP comic page");
            pix=fz_new_pixmap(ctx,fz_device_rgb(ctx),w,h,NULL,1);
            if(!WebPDecodeRGBAInto(data,size,fz_pixmap_samples(ctx,pix),(size_t)fz_pixmap_stride(ctx,pix)*h,fz_pixmap_stride(ctx,pix)))
                fz_throw(ctx,FZ_ERROR_GENERIC,"Cannot decode WebP comic page");
            for(int row=0;row<h;++row) {
                unsigned char *pixel=fz_pixmap_samples(ctx,pix)+row*fz_pixmap_stride(ctx,pix);
                for(int x=0;x<w;++x,pixel+=4)
                    for(int c=0;c<3;++c)pixel[c]=(unsigned char)((pixel[c]*pixel[3]+127)/255);
            }
            image=fz_new_image_from_pixmap(ctx,pix,NULL);
        } else image=fz_new_image_from_buffer(ctx,buffer);
        if(image->w<=0 || image->h<=0 || (uint64_t)image->w*image->h>100000000)
            fz_throw(ctx,FZ_ERROR_GENERIC,"Comic image dimensions exceed budget");
    }
    fz_always(ctx) {fz_drop_buffer(ctx,buffer);fz_drop_pixmap(ctx,pix);}
    fz_catch(ctx) {fz_drop_image(ctx,image);fz_rethrow(ctx);}
    return image;
}
static fz_rect comic_bounds(fz_context *ctx,fz_page *page_,fz_box_type box) {
    (void)box;ComicPage *page=(ComicPage *)page_;fz_image *image=page->image;
    uint8_t orientation=fz_image_orientation(ctx,image);
    return orientation==0 || (orientation&1)?fz_make_rect(0,0,image->w,image->h):fz_make_rect(0,0,image->h,image->w);
}
static void comic_run(fz_context *ctx,fz_page *page_,fz_device *device,fz_matrix ctm,fz_cookie *cookie) {
    (void)cookie;ComicPage *page=(ComicPage *)page_;fz_rect bounds=comic_bounds(ctx,page_,FZ_CROP_BOX);
    fz_matrix orientation=fz_post_scale(fz_image_orientation_matrix(ctx,page->image),bounds.x1,bounds.y1);
    fz_fill_image(ctx,device,page->image,fz_concat(orientation,ctm),1,fz_default_color_params);
}
static void comic_drop_page(fz_context *ctx,fz_page *page_) {fz_drop_image(ctx,((ComicPage *)page_)->image);}
static fz_page *comic_load_page(fz_context *ctx,fz_document *document,int chapter,int number) {
    (void)chapter;ComicDocument *doc=(ComicDocument *)document;ComicPage *page=NULL;fz_var(page);
    if(number<0 || number>=doc->count)fz_throw(ctx,FZ_ERROR_GENERIC,"Comic page is out of range");
    fz_try(ctx) {
        if(doc->cached_page!=number || !doc->cached_image) {
            fz_image *next=load_image(ctx,doc,number);
            fz_drop_image(ctx,doc->cached_image);doc->cached_image=next;doc->cached_page=number;
        }
        page=fz_new_derived_page(ctx,ComicPage,document);
        page->super.bound_page=comic_bounds;page->super.run_page_contents=comic_run;page->super.drop_page=comic_drop_page;
        page->image=fz_keep_image(ctx,doc->cached_image);
        fz_rect bounds=comic_bounds(ctx,(fz_page *)page,FZ_CROP_BOX);
        doc->entries[number].width=bounds.x1;doc->entries[number].height=bounds.y1;
    }
    fz_catch(ctx) {fz_drop_page(ctx,(fz_page *)page);fz_rethrow(ctx);}
    return (fz_page *)page;
}
static int comic_count(fz_context *ctx,fz_document *document,int chapter) {(void)ctx;(void)chapter;return ((ComicDocument *)document)->count;}
static int comic_metadata(fz_context *ctx,fz_document *document,const char *key,char *out,size_t capacity) {
    (void)ctx;ComicDocument *doc=(ComicDocument *)document;const char *value=NULL;
    if(!strcmp(key,"format"))value="Comic archive";
    else if(!strcmp(key,"info:Title"))value=doc->title;
    else if(!strcmp(key,"info:Author"))value=doc->author;
    else if(!strcmp(key,"info:Subject"))value=doc->genre;
    else if(!strcmp(key,"info:Series"))value=doc->series;
    else if(!strcmp(key,"info:SeriesIndex"))value=doc->number;
    else if(!strcmp(key,"info:ReadingDirection"))value=doc->manga?"rtl":"ltr";
    if(!value || !*value)return -1;
    if(out && capacity)copy_text(out,capacity,value);
    return (int)strlen(value)+1;
}
static void comic_drop(fz_context *ctx,fz_document *document) {
    ComicDocument *doc=(ComicDocument *)document;
    if(doc->cursor)archive_read_free(doc->cursor);
    if(doc->rar || doc->stream) {use_allocator(ctx);ar_close_archive(doc->rar);ar_close(doc->stream);reset_allocator();}
    for(int i=0;i<doc->count;++i)fz_free(ctx,doc->entries[i].name);
    fz_free(ctx,doc->entries);fz_free(ctx,doc->path);fz_drop_image(ctx,doc->cached_image);
}
fz_document *px_open_comic(fz_context *ctx,const char *path) {
    pthread_once(&comic_locale_once,comic_utf8_locale);
    ComicDocument *doc=fz_new_derived_document(ctx,ComicDocument);
    doc->super.drop_document=comic_drop;doc->super.count_pages=comic_count;
    doc->super.load_page=comic_load_page;doc->super.lookup_metadata=comic_metadata;doc->cached_page=-1;
    fz_try(ctx) {
        doc->path=fz_strdup(ctx,path);
        FILE *file=fopen(path,"rb");unsigned char signature[8]={0};
        if(!file)fz_throw(ctx,FZ_ERROR_GENERIC,"Cannot open comic file");
        size_t n=fread(signature,1,sizeof signature,file);fclose(file);
        doc->rar4=n>=7 && !memcmp(signature,"Rar!\x1a\x07\x00",7);
        if(doc->rar4)index_rar4(ctx,doc);else index_archive(ctx,doc);
    }
    fz_catch(ctx) {fz_drop_document(ctx,(fz_document *)doc);fz_rethrow(ctx);}
    return (fz_document *)doc;
}
int px_comic_cover_page(fz_document *document) {return ((ComicDocument *)document)->cover;}
int px_comic_page_is_spread(fz_document *document,int page) {
    ComicDocument *doc=(ComicDocument *)document;
    return page>=0 && page<doc->count && doc->entries[page].double_page;
}
int px_comic_toc(fz_context *ctx,fz_document *document,PxTocEntry *entries,int capacity) {
    (void)ctx;ComicDocument *doc=(ComicDocument *)document;
    int count=doc->outline_count?doc->outline_count:doc->count;if(count>capacity)count=capacity;
    for(int i=0;i<count;++i) {
        if(doc->outline_count)entries[i]=doc->outline[i];
        else {copy_text(entries[i].title,sizeof entries[i].title,doc->entries[i].name);entries[i].page=i;entries[i].level=0;entries[i].y=0;}
    }
    return count;
}

/* Geometry is memoized separately from the one-image decode cache. Continuous
 * scrolling and anchors must not repeatedly decompress preceding solid entries. */
int px_comic_page_size(fz_context *ctx,fz_document *document,int number,float *width,float *height) {
    ComicDocument *doc=(ComicDocument *)document;
    if(number<0 || number>=doc->count)return 0;
    ComicEntry *entry=&doc->entries[number];
    if(entry->height<=0) {fz_page *page=comic_load_page(ctx,document,0,number);fz_drop_page(ctx,page);}
    *width=entry->width;*height=entry->height;return *width>0 && *height>0;
}

/* Called only by this document's owner, with geometry from an independently
 * opened worker document. Avoid archive I/O for warmed neighbouring bounds. */
int px_comic_remember_size(fz_document *document,int page,float width,float height) {
    ComicDocument *doc=(ComicDocument *)document;
    if(page<0 || page>=doc->count || !(width>0 && height>0) || width>100000000.f/height)return 0;
    if(doc->entries[page].height<=0){doc->entries[page].width=width;doc->entries[page].height=height;}
    return 1;
}
