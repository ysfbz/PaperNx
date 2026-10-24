#include "papernx/cover_repository.hpp"
#include "papernx/mupdf_bridge.h"
#include <array>
#include <cassert>
#include <chrono>
#include <iostream>

using namespace papernx;
using Clock = std::chrono::steady_clock;

int main() {
    const auto root=fs::temp_directory_path()/
        ("papernx-cover-documents-"+std::to_string(Clock::now().time_since_epoch().count()));
    const std::array<fs::path,4> sources={"tests/fixtures/sample_book.epub","tests/fixtures/sample_doc.pdf",
        "tests/fixtures/comics/sample.cbz","tests/fixtures/comics/sample-rar4.cbr"};

    for(const int count:{100,500,1000}) {
        const auto caseRoot=root/std::to_string(count),booksRoot=caseRoot/"books",cacheRoot=caseRoot/"covers";
        fs::create_directories(booksRoot);
        std::vector<Book> books;books.reserve(count);
        for(int i=0;i<count;++i) {
            const auto &source=sources[std::size_t(i)%sources.size()];
            const auto path=booksRoot/("book-"+std::to_string(i)+source.extension().string());
            fs::copy_file(source,path);
            Book book;book.path=path.u8string();book.bytes=fs::file_size(path);
            book.id=documentId(book.path,book.bytes);book.format=detectFormat(path);books.push_back(std::move(book));
        }

        CoverRepository cold(cacheRoot);
        std::uint64_t coldOpens=0,metadataReads=0,firstColdMs=0;
        const auto coldStarted=Clock::now();
        auto decode=[&](const Book &book,CoverRepository &cache,bool readMetadata,std::uint64_t &opens) {
            char error[256]{};
            auto *doc=px_document_open_limited(book.path.c_str(),160,220,16.f,"",error,sizeof error,16u*1024u*1024u);
            if(!doc)return false;
            ++opens;
            if(readMetadata) {
                char title[256]{},author[256]{};
                px_document_metadata(doc,"info:Title",title,sizeof title);
                px_document_metadata(doc,"info:Author",author,sizeof author);
                ++metadataReads;
            }
            PxImage image{};
            bool ok=px_document_render_thumbnail(doc,150,210,&image,error,sizeof error) && image.rgb;
            if(ok) {
                CoverImage cached{image.width,image.height,image.stride,
                    std::vector<std::uint8_t>(image.rgb,image.rgb+std::size_t(image.stride)*image.height)};
                ok=cache.write(book,cached);
            }
            px_image_free(&image);px_document_close(doc);
            return ok;
        };
        for(std::size_t i=0;i<books.size();++i) {
            assert(!cold.read(books[i]));
            assert(decode(books[i],cold,true,coldOpens));
            if(i==0)firstColdMs=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-coldStarted).count();
        }
        const auto coldMs=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-coldStarted).count();
        const auto coldStats=cold.stats();
        assert(coldStats.misses==std::uint64_t(count) && coldStats.hits==0);
        assert(coldStats.entries<=512 && coldStats.bytes<=32u*1024u*1024u);

        CoverRepository warm(cacheRoot);
        std::uint64_t warmReopens=0,warmOpens=0,firstWarmMs=0;
        const auto warmStarted=Clock::now();
        for(std::size_t i=0;i<books.size();++i) {
            if(!warm.read(books[i])) {++warmReopens;assert(decode(books[i],warm,false,warmOpens));}
            if(i==0)firstWarmMs=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-warmStarted).count();
        }
        const auto warmMs=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-warmStarted).count();
        const auto warmStats=warm.stats();
        assert(warmStats.hits+warmStats.misses==std::uint64_t(count));
        assert(warmReopens==warmStats.misses && warmOpens==warmReopens && warmStats.entries<=512 && warmStats.bytes<=32u*1024u*1024u);
        std::cout<<count<<" real EPUB/PDF/CBZ/CBR documents: cold first cover="<<firstColdMs
                 <<" ms, cold fill="<<coldMs<<" ms, opens="<<coldOpens<<", metadata reads on same open="<<metadataReads
                 <<", cache="<<coldStats.entries<<" entries/"<<coldStats.bytes<<" bytes; warm first cover="<<firstWarmMs
                 <<" ms, warm pass="<<warmMs<<" ms, hits="<<warmStats.hits<<", misses/reopens="<<warmStats.misses
                 <<" ("<<warmOpens<<"), retained="<<warmStats.entries<<" entries/"<<warmStats.bytes<<" bytes\n";
    }

    std::error_code ec;fs::remove_all(root,ec);assert(!ec);
}
