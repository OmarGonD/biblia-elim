#include "backend/sqlite/sqlite_strong_lexicon.h"
#include "backend/strong_id.h"
#include <sqlite3.h>
struct SqliteStrongLexicon::Impl { sqlite3 *db=nullptr; ~Impl(){if(db)sqlite3_close(db);} };
static std::string col(sqlite3_stmt *s,int n){const unsigned char *p=sqlite3_column_text(s,n);return p?(const char*)p:"";}
SqliteStrongLexicon::SqliteStrongLexicon(const std::string &path):impl_(new Impl){sqlite3_open_v2(path.c_str(),&impl_->db,SQLITE_OPEN_READONLY,nullptr);}
SqliteStrongLexicon::~SqliteStrongLexicon()=default;
LexiconEntry SqliteStrongLexicon::lookupStrong(const StrongId &id) const {
 LexiconEntry r; r.id=id; if(!impl_->db)return r; sqlite3_stmt *s=nullptr;
 if(sqlite3_prepare_v2(impl_->db,"SELECT lemma,transliteration,pronunciation,definition FROM lexicon_entries WHERE strong=?",-1,&s,nullptr)!=SQLITE_OK)return r;
 std::string key=formatStrongId(id); sqlite3_bind_text(s,1,key.c_str(),-1,SQLITE_TRANSIENT);
 if(sqlite3_step(s)==SQLITE_ROW){r.lemma=col(s,0);r.transliteration=col(s,1);r.pronunciation=col(s,2);r.definition=col(s,3);r.valid=true;} sqlite3_finalize(s); return r;
}
