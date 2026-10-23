#include "papernx/library_index.hpp"
#include "papernx/app_settings.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace papernx;
static LibraryScanResult wait(LibraryScanner &scanner) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!scanner.ready() && std::chrono::steady_clock::now()<until)std::this_thread::yield();
    assert(scanner.ready());auto result=scanner.take();assert(result && !scanner.pending());return std::move(*result);
}
int main() {
    const auto root=fs::temp_directory_path()/"papernx-index-tests";fs::remove_all(root);fs::create_directories(root/"books"/"nested");
    for(int i=0;i<300;++i)std::ofstream(root/"books"/"nested"/("issue-"+std::to_string(i)+".cbz"))<<"fixture";
    std::ofstream(root/"books"/"ignored.txt")<<"ignored";
    const auto inventory=root/"data"/"inventory.txt";LibraryScanner scanner;
    assert(scanner.start({root/"books"/"nested",root/"books",root/"books"}, {}, inventory));
    assert(!scanner.start({}, {}, inventory));auto first=wait(scanner);assert(first.cached && first.error.empty() && first.books.size()==300);
    auto cached=loadLibraryInventory(inventory);assert(cached.size()==300);
    auto known=first.books;known.front().title="Custom title";known.front().author="Author";
    const auto changed=known.back();std::ofstream(changed.path,std::ios::app)<<"new bytes";
    fs::remove(known[1].path);
    assert(scanner.start({root/"books"},known,inventory));auto second=wait(scanner);assert(second.books.size()==299 && second.cached);
    bool custom=false,replaced=false;
    for(const auto &book:second.books){assert(book.id!=known[1].id);if(book.id==known.front().id)custom=book.title=="Custom title" && book.author=="Author";if(book.path==changed.path)replaced=book.id!=changed.id && book.bytes==16;}
    assert(custom && replaced);
    // Removing a scan root retains already-opened, existing files, without touching progress.
    assert(scanner.start({},second.books,inventory));assert(wait(scanner).books.size()==299);
    assert(scanner.start({root/"books"},{},inventory));scanner.stop();assert(!scanner.ready() && !scanner.pending());
    assert(loadLibraryInventory(inventory).size()==299);
    assert(scan(root/"books",5).size()==5);assert(scan(root/"books",100,[]{return true;}).empty());
    AppSettingsStore settings(root/"settings.txt");auto prefs=settings.get();assert(prefs.scanOnLaunch && prefs.interfaceStyle==0 && prefs.interfaceAccent==0);
    prefs.libraryFolders={"/books/a","/books/a","/books/b"};prefs.interfaceStyle=1;prefs.interfaceAccent=5;prefs.scanOnLaunch=false;settings.put(prefs);assert(settings.save());
    AppSettingsStore restored(root/"settings.txt");assert(restored.load());assert(restored.get().libraryFolders.size()==2 && !restored.get().scanOnLaunch && restored.get().interfaceStyle==1 && restored.get().interfaceAccent==5);
    fs::remove_all(root);std::cout<<"PASS: batch inventory, nested root deduplication, changed/missing files, atomic cache, cancellation and persisted scan/theme preferences\n";
}
