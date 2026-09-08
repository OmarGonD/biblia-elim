#include "backend/osis_importer.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <cassert>
int main(){gchar *d=g_dir_make_tmp("osis-test-XXXXXX",nullptr);assert(d); UsfmImportOptions o; o.moduleId="osis";o.name="OSIS";o.language="en";o.versification="custom"; UsfmImportStats s;std::string e; std::string out=std::string(d)+"/osis.sqlite"; assert(importOsis(std::string(SRCDIR)+"/tests/fixtures/osis/basic.xml",out,o,s,e)); SqliteBibleBackend b(d); assert(b.hasModule("osis")); BibleKeyInfo k; assert(b.resolveKey("osis","John 3:16",k)); auto c=b.getVerseContent("osis",k.reference); assert(c.valid&&!c.plainText.empty()); assert(c.footnotes.size()==1); g_remove(out.c_str()); std::string milestone=std::string(d)+"/milestone.sqlite"; o.moduleId="milestone"; assert(importOsis(std::string(SRCDIR)+"/tests/fixtures/osis/milestone.xml",milestone,o,s,e)); SqliteBibleBackend bm(d); assert(bm.hasModule("milestone")); g_remove(milestone.c_str());g_rmdir(d);g_free(d);}
