#include "backend/sqlite/sqlite_module_writer.h"
#include "backend/morphology.h"
#include "backend/strong_id.h"
#include <sqlite3.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <map>

namespace {
bool exec(sqlite3 *db, const char *sql, std::string &error) {
    char *message = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &message) != SQLITE_OK) {
        error = message ? message : "SQLite error"; sqlite3_free(message); return false;
    }
    return true;
}
bool valid(const SqliteImportVerse &v, std::string &error) {
    if (v.reference.book <= 0 || v.reference.chapter <= 0 || v.reference.verse <= 0) { error = "invalid verse reference"; return false; }
    for (const auto &w : v.words) {
        if (w.start > v.text.size() || w.length > v.text.size() - w.start || v.text.substr(w.start, w.length) != w.text) { error = "invalid word offset"; return false; }
        for (const MorphologyTag &tag : w.morphologyTags) {
            if (!isValidMorphologyTag(tag)) { error = "invalid morphology tag"; return false; }
        }
    }
    for (const auto &s : v.spans) if (s.start > v.text.size() || s.length > v.text.size() - s.start) { error = "invalid span offset"; return false; }
    for (const auto &n : v.footnotes) if (n.offset > v.text.size()) { error = "invalid footnote offset"; return false; }
    for (const auto &x : v.crossReferences) if (x.offset > v.text.size()) { error = "invalid cross-reference offset"; return false; }
    return true;
}
void bindText(sqlite3_stmt *s, int n, const std::string &v) { sqlite3_bind_text(s, n, v.c_str(), -1, SQLITE_TRANSIENT); }
}

bool SqliteModuleWriter::write(const SqliteModuleMetadata &m,
    const std::vector<SqliteImportBook> &books,
    const std::vector<SqliteImportVerse> &verses,
    const std::string &output, std::string &error) const {
    if (m.moduleId.empty() || m.name.empty() || m.language.empty() || (m.versification != "kjv" && m.versification != "custom")) { error = "invalid module metadata"; return false; }
    if (books.empty() || verses.empty()) { error = "module has no books or verses"; return false; }
    for (const auto &v : verses) if (!valid(v, error)) return false;
    const std::string tmp = output + ".tmp"; std::remove(tmp.c_str());
    sqlite3 *db = nullptr;
    if (sqlite3_open(tmp.c_str(), &db) != SQLITE_OK) { error = "cannot create output"; if (db) sqlite3_close(db); return false; }
    bool ok = exec(db, "PRAGMA user_version=1; PRAGMA foreign_keys=ON; BEGIN IMMEDIATE;", error);
    ok = ok && exec(db, "CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL); CREATE TABLE books(book_id INTEGER PRIMARY KEY,osis TEXT NOT NULL UNIQUE,name TEXT NOT NULL,short_name TEXT,testament INTEGER NOT NULL,position INTEGER NOT NULL UNIQUE); CREATE TABLE verses(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL CHECK(chapter>0),verse INTEGER NOT NULL CHECK(verse>0),text TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse),FOREIGN KEY(book_id) REFERENCES books(book_id)); CREATE INDEX verses_book_chapter ON verses(book_id,chapter,verse); CREATE TABLE verse_annotations(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,paragraph_break INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(book_id,chapter,verse),FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse)); CREATE TABLE headings(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,sequence INTEGER NOT NULL,text TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,sequence),FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse)); CREATE TABLE verse_spans(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,start INTEGER NOT NULL,length INTEGER NOT NULL,style TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,start,style),FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse)); CREATE TABLE verse_words(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,sequence INTEGER NOT NULL,start INTEGER NOT NULL,length INTEGER NOT NULL,text TEXT NOT NULL,strong TEXT,PRIMARY KEY(book_id,chapter,verse,sequence),FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse)); CREATE TABLE verse_word_morphology(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,word_sequence INTEGER NOT NULL,morphology_sequence INTEGER NOT NULL,scheme TEXT NOT NULL,code TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,word_sequence,morphology_sequence),FOREIGN KEY(book_id,chapter,verse,word_sequence) REFERENCES verse_words(book_id,chapter,verse,sequence)); CREATE INDEX verse_word_morphology_lookup ON verse_word_morphology(scheme,code,book_id,chapter,verse,word_sequence,morphology_sequence); CREATE TABLE verse_word_strongs(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,sequence INTEGER NOT NULL,strong TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,sequence,strong)); CREATE INDEX verse_word_strongs_canonical ON verse_word_strongs(strong,book_id,chapter,verse,sequence); CREATE VIRTUAL TABLE verses_fts USING fts5(text,content='verses',content_rowid='rowid');", error);
    ok = ok && exec(db, "CREATE TABLE footnotes(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,sequence INTEGER NOT NULL,offset INTEGER NOT NULL,caller TEXT,body TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,sequence)); CREATE TABLE cross_references(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,sequence INTEGER NOT NULL,offset INTEGER NOT NULL,display_text TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,sequence)); CREATE TABLE cross_reference_targets(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,sequence INTEGER NOT NULL,target_sequence INTEGER NOT NULL,target_book_id INTEGER NOT NULL,target_chapter INTEGER NOT NULL,target_verse INTEGER NOT NULL,PRIMARY KEY(book_id,chapter,verse,sequence,target_sequence));", error);
    sqlite3_stmt *s = nullptr;
    bool hs=false,hm=false,hf=false,hx=false; for (const auto &v:verses) { hf|=!v.footnotes.empty(); hx|=!v.crossReferences.empty(); for(const auto&w:v.words) { hs|=!w.strongs.empty(); hm|=!w.morphologyTags.empty(); } }
    bool hh=false; for(const auto&v:verses) hh |= !v.headings.empty();
    std::map<std::string,std::string> md={{"schema_version","1"},{"module_id",m.moduleId},{"name",m.name},{"language",m.language},{"module_type","bible"},{"versification",m.versification},{"source_format",m.sourceFormat},{"feature.verses","true"},{"feature.search","true"},{"feature.strong",hs?"true":"false"},{"feature.morphology",hm?"true":"false"},{"feature.headings",hh?"true":"false"},{"feature.footnotes",hf?"true":"false"},{"feature.crossrefs",hx?"true":"false"},{"feature.dictionary","false"}};
    if(!m.abbreviation.empty())md["abbreviation"]=m.abbreviation; if(!m.description.empty())md["description"]=m.description; if(!m.license.empty())md["license"]=m.license; if(!m.publisher.empty())md["publisher"]=m.publisher; if(!m.source.empty())md["source"]=m.source; if(!m.contentVersion.empty())md["content_version"]=m.contentVersion;
    if (ok && sqlite3_prepare_v2(db,"INSERT INTO metadata(key,value) VALUES(?,?)",-1,&s,nullptr)==SQLITE_OK) { for(auto &p:md){bindText(s,1,p.first);bindText(s,2,p.second);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);} } else ok=false; if(s){sqlite3_finalize(s);s=nullptr;}
    if(ok&&sqlite3_prepare_v2(db,"INSERT INTO books VALUES(?,?,?,?,?,?)",-1,&s,nullptr)==SQLITE_OK){for(const auto&b:books){sqlite3_bind_int(s,1,b.id);bindText(s,2,b.osis);bindText(s,3,b.name);bindText(s,4,b.shortName);sqlite3_bind_int(s,5,b.testament);sqlite3_bind_int(s,6,b.position);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}}else ok=false;if(s){sqlite3_finalize(s);s=nullptr;}
    auto insert = [&](const char *sql, auto fn){ if(!ok)return; if(sqlite3_prepare_v2(db,sql,-1,&s,nullptr)!=SQLITE_OK){ok=false;return;} fn(); sqlite3_finalize(s);s=nullptr;};
    insert("INSERT INTO verses VALUES(?,?,?,?)", [&]{for(const auto&v:verses){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);bindText(s,4,v.text);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}});
    insert("INSERT INTO verse_annotations VALUES(?,?,?,?)", [&]{for(const auto&v:verses)if(v.paragraphBreak){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);sqlite3_bind_int(s,4,1);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}});
    insert("INSERT INTO headings VALUES(?,?,?,?,?)", [&]{for(const auto&v:verses){int q=0;for(const auto&h:v.headings){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);sqlite3_bind_int(s,4,q++);bindText(s,5,h.text);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}}});
    insert("INSERT INTO verse_spans VALUES(?,?,?,?,?,?)", [&]{for(const auto&v:verses)for(const auto&x:v.spans){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);sqlite3_bind_int(s,4,x.start);sqlite3_bind_int(s,5,x.length);bindText(s,6,"added");if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}});
    insert("INSERT INTO verse_words VALUES(?,?,?,?,?,?,?,?)", [&]{for(const auto&v:verses){int q=0;for(const auto&w:v.words){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);sqlite3_bind_int(s,4,q++);sqlite3_bind_int(s,5,w.start);sqlite3_bind_int(s,6,w.length);bindText(s,7,w.text);if(w.strong.empty())sqlite3_bind_null(s,8);else bindText(s,8,w.strong);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}}});
    insert("INSERT INTO verse_word_morphology VALUES(?,?,?,?,?,?,?)", [&]{for(const auto&v:verses){int q=0;for(const auto&w:v.words){int t=0;for(const auto&tag:w.morphologyTags){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);sqlite3_bind_int(s,4,q);sqlite3_bind_int(s,5,t++);bindText(s,6,tag.scheme);bindText(s,7,tag.code);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}++q;}}});
    insert("INSERT INTO footnotes VALUES(?,?,?,?,?,?,?)", [&]{for(const auto&v:verses){int q=0;for(const auto&n:v.footnotes){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);sqlite3_bind_int(s,4,q++);sqlite3_bind_int(s,5,n.offset);bindText(s,6,n.label);bindText(s,7,n.body);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}}});
    insert("INSERT INTO cross_references VALUES(?,?,?,?,?,?)", [&]{for(const auto&v:verses){int q=0;for(const auto&x:v.crossReferences){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);sqlite3_bind_int(s,4,q++);sqlite3_bind_int(s,5,x.offset);bindText(s,6,x.displayText);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}}});
    insert("INSERT INTO cross_reference_targets VALUES(?,?,?,?,?,?,?,?)", [&]{for(const auto&v:verses){int q=0;for(const auto&x:v.crossReferences){int t=0;for(const auto&r:x.references){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);sqlite3_bind_int(s,4,q);sqlite3_bind_int(s,5,t++);sqlite3_bind_int(s,6,r.book);sqlite3_bind_int(s,7,r.chapter);sqlite3_bind_int(s,8,r.verse);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}++q;}}});
    insert("INSERT OR IGNORE INTO verse_word_strongs VALUES(?,?,?,?,?)", [&]{for(const auto&v:verses){int q=0;for(const auto&w:v.words){for(const auto&id:w.strongs){sqlite3_bind_int(s,1,v.reference.book);sqlite3_bind_int(s,2,v.reference.chapter);sqlite3_bind_int(s,3,v.reference.verse);sqlite3_bind_int(s,4,q);auto strong=formatStrongId(id);bindText(s,5,strong);if(sqlite3_step(s)!=SQLITE_DONE){ok=false;break;}sqlite3_reset(s);sqlite3_clear_bindings(s);}++q;}}});
    if(ok)ok=exec(db,"INSERT INTO verses_fts(rowid,text) SELECT rowid,text FROM verses; COMMIT;",error);else exec(db,"ROLLBACK;",error); sqlite3_close(db); if(!ok){std::remove(tmp.c_str());if(error.empty())error="failed writing SQLite module";return false;} if(std::rename(tmp.c_str(),output.c_str())!=0){error="cannot finalize output: "+std::string(std::strerror(errno));std::remove(tmp.c_str());return false;} return true;
}
