#include "papernx/orientation.hpp"
#include "papernx/pdf_gestures.hpp"
#include "papernx/app_settings.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
int main() {
    using namespace papernx;
    for(int o=0;o<3;++o) {
        DisplayTransform t{1280,720,o};
        assert(t.logicalWidth()==(o?720:1280));
        auto direction=t.vectorToLogical(1,0);
        assert(direction == (o==0?std::pair<int,int>{1,0}:o==1?std::pair<int,int>{0,-1}:std::pair<int,int>{0,1}));
        auto up=t.vectorToLogical(0,-1);
        assert(up == (o==0?std::pair<int,int>{0,-1}:o==1?std::pair<int,int>{-1,0}:std::pair<int,int>{1,0}));
        for(auto p:{std::pair<float,float>{0,0},{200,350},{1280,720}}) {
            auto l=t.toLogical(p.first,p.second),r=t.toPhysical(l.first,l.second);
            assert(std::abs(r.first-p.first)<.01f && std::abs(r.second-p.second)<.01f);
        }
    }
    assert(classifyPdfGesture({-240,15,1280,230})==PdfGesture::PageNext);
    assert(classifyPdfGesture({240,15,1280,230})==PdfGesture::PagePrevious);
    assert(classifyPdfGesture({-240,15,1280,1100})==PdfGesture::Pan);
    assert(classifyPdfGesture({-240,180,1280,230})==PdfGesture::Pan);
    assert(classifyPdfGesture({-240,15,1280,230,true})==PdfGesture::Pinch);
    assert(classifyPdfGesture({-240,15,1280,230,false,true})==PdfGesture::Ignore);
    assert(classifyPdfGesture({5,5,1280,30})==PdfGesture::Tap);
    assert(classifyPdfGesture({-40,0,1280,30})==PdfGesture::Pan);
    assert(classifyPdfGesture({-240,15,1280,230,false,false,-1.1f})==PdfGesture::PageNext);
    assert(classifyPdfGesture({-240,15,1280,230,false,false,0.05f})==PdfGesture::Pan);
    assert(classifyPdfGesture({-240,15,1280,230,false,false,1.1f})==PdfGesture::Pan); // Opposite release direction must not flip a page.
    const auto path=std::filesystem::temp_directory_path()/"papernx-foundations-settings.db";
    std::filesystem::remove(path); std::filesystem::remove(path.string()+".bak");
    AppSettingsStore store(path); AppPreferences pref;
    pref.uiTheme=2; pref.chromePolicy=2; pref.customUiTheme=5; pref.displayOrientation=2;
    pref.preservePdfZoom=false; pref.paperTexture=2;
    store.put(pref); assert(store.save());
    AppSettingsStore loaded(path); assert(loaded.load());
    assert(loaded.get().uiTheme==2 && loaded.get().chromePolicy==0 && loaded.get().customUiTheme==5); // Legacy custom chrome is ignored.
    assert(loaded.get().displayOrientation==2 && !loaded.get().preservePdfZoom && loaded.get().paperTexture==2);
    std::filesystem::remove(path); std::filesystem::remove(path.string()+".bak");
}
