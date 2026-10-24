#include "papernx/library_index.hpp"
#include "papernx/app_settings.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace papernx;
static LibraryScanResult wait(LibraryScanner &scanner) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!scanner.ready() && std::chrono::steady_clock::now()<until)std::this_thread::yield();
    assert(scanner.ready());auto result=scanner.take();assert(result && !scanner.pending());return std::move(*result);
}
int main() {
    const auto root=fs::temp_directory_path()/"papernx-index-tests";fs::remove_all(root);fs::create_directories(root/"books"/"nested");
    for(int i=0;i<300;++i)std::ofstream(root/"books"/"nested"/("issue-"+std::to_string(i)+".cbz"))<<"fixture";
    const auto unicodeRoot=root/fs::u8path(u8"Türkçe kitaplar");
    const auto unicodeBook=unicodeRoot/fs::u8path(u8"İçerik ı-Çözüm.cbz");
    fs::create_directories(unicodeRoot);std::ofstream(unicodeBook)<<"fixture";
    std::ofstream(root/"books"/"ignored.txt")<<"ignored";
    const auto inventory=root/"data"/"inventory.txt";LibraryScanner scanner;
    const auto deepRoot=root/"deep";
    auto withinLimit=deepRoot/"within-limit";
    for(int i=0;i<15;++i){withinLimit/="nested";fs::create_directories(withinLimit);}
    std::ofstream(withinLimit/"within.epub")<<"within depth limit";
    auto beyondLimit=deepRoot/"beyond-limit";
    for(int i=0;i<18;++i){beyondLimit/="nested";fs::create_directories(beyondLimit);}
    std::ofstream(beyondLimit/"beyond.epub")<<"beyond depth limit";
    bool legacyScanComplete=false;
    assert(scan(deepRoot,100,{},&legacyScanComplete).size()==1&&!legacyScanComplete);
    bool rootsScanComplete=true;
    assert(scanRoots({deepRoot},100,{},&rootsScanComplete).size()==1&&!rootsScanComplete);
    const auto capRoot=root/"book-limit";fs::create_directories(capRoot);
    for(int i=0;i<10000;++i){std::ofstream file(capRoot/("book-"+std::to_string(i)+".cbz"));}
    bool exactBookCapComplete=false;
    assert(scanRoots({capRoot},10000,{},&exactBookCapComplete).size()==10000&&exactBookCapComplete);
    {std::ofstream extra(capRoot/"book-10000.cbz");}
    bool exceededBookCapComplete=true;
    assert(scanRoots({capRoot},10000,{},&exceededBookCapComplete).size()==10000&&!exceededBookCapComplete);
    assert(scanner.start({root/"books"/"nested",root/"books",root/"books"}, {}, inventory));
    assert(!scanner.start({}, {}, inventory));auto first=wait(scanner);assert(first.complete && first.cached && first.error.empty() && first.books.size()==300);
    assert(scanRoots({root/"books"/"nested",root/"books",root/"books"}).size()==300);
    bool rootsComplete=true;assert(scanRoots({root/"books",root/"missing-root"},100,{},&rootsComplete).size()==100 && !rootsComplete);
    bool limitComplete=true;assert(scanRoots({root/"books"},5,{},&limitComplete).size()==5 && !limitComplete);
    bool exactLimitComplete=false;assert(scanRoots({root/"books"/"nested"},300,{},&exactLimitComplete).size()==300 && exactLimitComplete);
    bool multiLimitComplete=true;assert(scanRoots({root/"books"/"nested",unicodeRoot},300,{},&multiLimitComplete).size()==300 && !multiLimitComplete);
    const auto unicodeBooks=scan(unicodeRoot);
    assert(unicodeBooks.size()==1 && unicodeBooks.front().path==unicodeBook.u8string() &&
           unicodeBooks.front().title==unicodeBook.stem().u8string());
    LibraryScanner unicodeScanner;const auto unicodeInventory=root/"data"/"unicode-inventory.txt";
    assert(unicodeScanner.start({unicodeRoot},{},unicodeInventory));
    auto unicodeResult=wait(unicodeScanner);
    assert(unicodeResult.complete && unicodeResult.cached && unicodeResult.books.size()==1 &&
           unicodeResult.books.front().path==unicodeBook.u8string());
    const auto restoredUnicode=loadLibraryInventory(unicodeInventory);
    assert(restoredUnicode.size()==1 && restoredUnicode.front().id==unicodeResult.books.front().id &&
           restoredUnicode.front().path==unicodeBook.u8string());
    ProgressStore unicodeIndex(unicodeInventory);assert(unicodeIndex.load() && unicodeIndex.get("libraryindex0001").bookTitle=="PAPERNX-LIBRARY-1");
    const auto legacyInventory=root/"data"/"legacy-inventory.txt";ProgressStore legacyIndex(legacyInventory);
    Progress legacyRecord;legacyRecord.bookPath=unicodeResult.books.front().path;legacyRecord.bookTitle=unicodeResult.books.front().title;
    legacyRecord.bookAuthor=std::to_string(unicodeResult.books.front().bytes);legacyRecord.lastRead=unicodeResult.books.front().modified;
    legacyIndex.put(unicodeResult.books.front().id,legacyRecord);assert(legacyIndex.save());
    const auto migratedLegacy=loadLibraryInventory(legacyInventory);
    assert(migratedLegacy.size()==1 && migratedLegacy.front().id==unicodeResult.books.front().id);
    const auto futureInventory=root/"data"/"future-inventory.txt";ProgressStore futureIndex(futureInventory);
    Progress futureMarker;futureMarker.bookTitle="PAPERNX-LIBRARY-2";futureIndex.put("libraryindex0001",futureMarker);
    futureIndex.put(unicodeResult.books.front().id,legacyRecord);assert(futureIndex.save());
    assert(loadLibraryInventory(futureInventory).empty());
    const auto brokenMarkerInventory=root/"data"/"broken-marker-inventory.txt";ProgressStore brokenMarkerIndex(brokenMarkerInventory);
    brokenMarkerIndex.put("libraryindex0001",Progress{});brokenMarkerIndex.put(unicodeResult.books.front().id,legacyRecord);
    assert(brokenMarkerIndex.save() && loadLibraryInventory(brokenMarkerInventory).empty());
    const auto validationRoot=root/"inventory-validation";fs::create_directories(validationRoot);
    const auto keepFile=validationRoot/"keep.cbz",changedFile=validationRoot/"changed.cbz",missingFile=validationRoot/"missing.cbz";
    const auto outsideFile=root/"outside.cbz";
    std::ofstream(keepFile)<<"keep";std::ofstream(changedFile)<<"old";std::ofstream(missingFile)<<"gone";std::ofstream(outsideFile)<<"outside";
    const auto validationInventory=root/"data"/"validated-inventory.txt";LibraryScanner validationScanner;
    assert(validationScanner.start({validationRoot},{},validationInventory));
    const auto validation=wait(validationScanner);assert(validation.complete && validation.cached && validation.books.size()==3);
    std::ofstream(changedFile,std::ios::trunc)<<"changed size";fs::remove(missingFile);
    std::error_code symlinkError;fs::create_symlink(outsideFile,missingFile,symlinkError);
    assert(!symlinkError);
    const auto validatedInventory=loadLibraryInventory(validationInventory);
    assert(validatedInventory.size()==1 && validatedInventory.front().path==keepFile.u8string());
    Book symlinkHistory;symlinkHistory.path=missingFile.u8string();symlinkHistory.bytes=fs::file_size(outsideFile);
    symlinkHistory.id=documentId(symlinkHistory.path,symlinkHistory.bytes);symlinkHistory.format=Format::Cbz;
    LibraryScanner retainedSymlinkScanner;const auto retainedSymlinkInventory=root/"data"/"retained-symlink.txt";
    assert(retainedSymlinkScanner.start({}, {symlinkHistory}, retainedSymlinkInventory));
    const auto retainedSymlink=wait(retainedSymlinkScanner);
    assert(retainedSymlink.complete && retainedSymlink.books.empty());
    assert(scanRoots({root/"books"},5).size()==5);
    auto cached=loadLibraryInventory(inventory);assert(cached.size()==300);
    auto inventoryBytes=[&]{std::ifstream in(inventory,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};
    const auto inventoryBeforeDepthLimit=inventoryBytes();
    assert(scanner.start({deepRoot},first.books,inventory));
    auto depthLimited=wait(scanner);
    assert(!depthLimited.complete&&!depthLimited.error.empty());
    assert(inventoryBytes()==inventoryBeforeDepthLimit&&loadLibraryInventory(inventory).size()==300);
    auto known=first.books;known.front().title="Custom title";known.front().author="Author";
    const auto changed=known.back();std::ofstream(changed.path,std::ios::app)<<"new bytes";
    fs::remove(known[1].path);
    assert(scanner.start({root/"books"},known,inventory));auto second=wait(scanner);assert(second.books.size()==299 && second.cached);
    bool custom=false,replaced=false;
    for(const auto &book:second.books){assert(book.id!=known[1].id);if(book.id==known.front().id)custom=book.title=="Custom title" && book.author=="Author";if(book.path==changed.path)replaced=book.id!=changed.id && book.bytes==16;}
    assert(custom && replaced);
    // Removing a scan root retains already-opened, existing files, without touching progress.
    assert(scanner.start({},second.books,inventory));assert(wait(scanner).books.size()==299);
    const auto savedInventory=inventoryBytes();
    const auto inventoryBeforeCancel=loadLibraryInventory(inventory);
    assert(scanner.start({root/"books",root/"missing-sd-library"},second.books,inventory));
    auto incomplete=wait(scanner);assert(!incomplete.complete && !incomplete.error.empty());
    const auto inventoryAfterFailure=loadLibraryInventory(inventory);
    assert(inventoryAfterFailure.size()==inventoryBeforeCancel.size());
    for(std::size_t i=0;i<inventoryBeforeCancel.size();++i)
        assert(inventoryAfterFailure[i].id==inventoryBeforeCancel[i].id && inventoryAfterFailure[i].path==inventoryBeforeCancel[i].path);
    assert(inventoryBytes()==savedInventory);
    assert(scanner.start({root/"books"},second.books,inventory));scanner.cancel();
    auto cancelled=wait(scanner);assert(cancelled.cancelled);
    assert(loadLibraryInventory(inventory).size()==inventoryBeforeCancel.size());
    assert(inventoryBytes()==savedInventory);
    assert(scanner.start({root/"books"},second.books,inventory));
    auto restarted=wait(scanner);assert(!restarted.cancelled && restarted.books.size()==299);
    assert(scanner.start({root/"books"},second.books,inventory));
    const auto completedDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!scanner.ready() && std::chrono::steady_clock::now()<completedDeadline)std::this_thread::yield();
    assert(scanner.ready());scanner.cancel();
    auto completedThenCancelled=scanner.take();
    assert(completedThenCancelled && completedThenCancelled->complete && !completedThenCancelled->cancelled);
    assert(scanner.start({root/"books"},{},inventory));scanner.stop();assert(!scanner.ready() && !scanner.pending());
    assert(loadLibraryInventory(inventory).size()==299);
    const auto broken=root/"data"/"broken-inventory.txt";std::ofstream(broken)<<"damaged inventory";
    assert(loadLibraryInventory(broken).empty());
    ProgressStore reading(root/"data"/"reading-progress.txt");Progress progress;progress.page=7;progress.bookPath=changed.path;reading.put(changed.id,progress);assert(reading.save());
    assert(scanner.start({root/"books"},second.books,broken));auto recovered=wait(scanner);
    assert(recovered.cached && recovered.books.size()==299 && loadLibraryInventory(broken).size()==299);
    ProgressStore restoredReading(root/"data"/"reading-progress.txt");assert(restoredReading.load() && restoredReading.get(changed.id).page==7);
    assert(scan(root/"books",5).size()==5);
    bool cancelledScanComplete=true;
    assert(scan(root/"books",100,[]{return true;},&cancelledScanComplete).empty()&&!cancelledScanComplete);
    std::size_t checks=0;const auto interrupted=scan(root/"books",300,[&]{return ++checks==12;});
    assert(checks==12 && interrupted.size()<300);
    checks=0;bool interruptedRootsComplete=true;
    const auto multiRootInterrupted=scanRoots({root/"books",root/"books"/"nested"},300,[&]{return ++checks==12;},&interruptedRootsComplete);
    assert(checks==12 && multiRootInterrupted.size()<300&&!interruptedRootsComplete);
    AppSettingsStore settings(root/"settings.txt");auto prefs=settings.get();assert(prefs.scanOnLaunch && prefs.interfaceStyle==0 && prefs.interfaceAccent==0);
    prefs.libraryFolders={"/books/a","/books/a","/books/b"};prefs.interfaceStyle=1;prefs.interfaceAccent=5;prefs.scanOnLaunch=false;settings.put(prefs);assert(settings.save());
    AppSettingsStore restored(root/"settings.txt");assert(restored.load());assert(restored.get().libraryFolders.size()==2 && !restored.get().scanOnLaunch && restored.get().interfaceStyle==1 && restored.get().interfaceAccent==5);
    fs::remove_all(root);std::cout<<"PASS: multi-root scan, nested-root deduplication, corrupt inventory recovery, progress isolation, cancellation and persisted preferences\n";
}
