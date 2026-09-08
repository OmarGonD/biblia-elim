#ifndef XIPHOS_STRONG_ID_H
#define XIPHOS_STRONG_ID_H
#include <string>
#include <vector>
#include "backend/bible_types.h"
bool parseStrongId(const std::string &, StrongId &);
std::vector<StrongId> parseStrongIds(const std::string &);
std::string formatStrongId(const StrongId &);
#endif
