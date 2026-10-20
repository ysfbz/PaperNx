#pragma once
#include "papernx/catalog.hpp"

namespace papernx {
struct SeriesIdentity { std::string key,title,issue; };
SeriesIdentity identifySeries(const Book &book,const CatalogEntry &entry);
bool seriesIssueLess(const Book &a,const Book &b,const CatalogStore &catalog);
}
