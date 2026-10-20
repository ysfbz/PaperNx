#include "papernx/series.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace papernx {
namespace {
std::string seriesFold(std::string text) {
    for(char &c:text)if(c>='A' && c<='Z')c=char(c-'A'+'a');
    return turkishCaseFold(text);
}
std::string trimSeries(std::string text) {
    while(!text.empty() && (std::isspace(static_cast<unsigned char>(text.back())) || text.back()=='#' || text.back()=='-' || text.back()=='_' || text.back()=='.'))text.pop_back();
    auto fold=seriesFold(text);
    for(const char *suffix:{" issue"," no"," number"," #"," volume"," vol"," cilt"," sayi"," sayı"," v"}) {
        std::string s=suffix;
        if(fold.size()>=s.size() && fold.compare(fold.size()-s.size(),s.size(),s)==0) {
            text.resize(text.size()-s.size());
            break;
        }
    }
    while(!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))text.pop_back();
    return text;
}
SeriesIdentity infer(const std::string &text) {
    for(std::size_t i=0;i<text.size();++i) {
        if(text[i]<'0' || text[i]>'9')continue;
        auto end=i;while(end<text.size() && ((text[end]>='0' && text[end]<='9') || text[end]=='.'))++end;
        // Require an issue boundary. A year in a release tag is not an issue.
        const bool vPrefix=i>=4 && (text[i-1]=='v' || text[i-1]=='V') && (text[i-2]==' ' || text[i-2]=='_' || text[i-2]=='-');
        if(i<3 || (!vPrefix && text[i-1]!=' ' && text[i-1]!='#' && text[i-1]!='_' && text[i-1]!='-' && text[i-1]!='.')) {i=end-1;continue;}
        auto title=trimSeries(text.substr(0,i));
        if(title.size()<3 || title.back()=='(' || title.back()=='[') {i=end-1;continue;}
        auto issue=text.substr(i,end-i);
        while(!issue.empty() && issue.back()=='.')issue.pop_back();
        if(issue.size()==4 && issue>="1900" && issue<="2099" && text[i-1]!='#') {i=end-1;continue;}
        return {{},title,issue};
    }
    return {};
}
bool number(const std::string &text,double &value) {
    if(text.empty())return false;
    char *end=nullptr;value=std::strtod(text.c_str(),&end);
    return end!=text.c_str() && !*end && std::isfinite(value);
}
}
SeriesIdentity identifySeries(const Book &book,const CatalogEntry &entry) {
    SeriesIdentity result;
    const bool current=catalogMetadataCurrent(book,entry);
    result.title=entry.series.empty()?(current?entry.sourceSeries:std::string{}):entry.series;
    result.issue=current?entry.sourceSeriesIndex:std::string{};
    if(isComic(book.format)) {
        auto guessed=infer(book.title);
        if(guessed.title.empty())guessed=infer(defaultTitle(fs::u8path(book.path)));
        if(result.title.empty())result.title=guessed.title;
        if(result.issue.empty())result.issue=guessed.issue;
    }
    std::replace(result.title.begin(),result.title.end(),'_',' ');
    std::istringstream words(result.title);std::string word,normalized;
    while(words>>word) {if(!normalized.empty())normalized+=' ';normalized+=word;}
    result.title=normalized;result.key=seriesFold(normalized);
    return result;
}
bool seriesIssueLess(const Book &a,const Book &b,const CatalogStore &catalog) {
    const auto left=identifySeries(a,catalog.get(a.id)),right=identifySeries(b,catalog.get(b.id));
    double ln=0,rn=0;bool la=number(left.issue,ln),ra=number(right.issue,rn);
    if(la && ra && ln!=rn)return ln<rn;
    if(left.issue.empty()!=right.issue.empty())return !left.issue.empty();
    int order=naturalCompare(left.issue,right.issue);
    if(order)return order<0;
    order=naturalCompare(a.title,b.title);if(order)return order<0;
    order=naturalCompare(a.path,b.path);return order?order<0:a.id<b.id;
}
}
