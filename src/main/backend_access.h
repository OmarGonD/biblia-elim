#ifndef BIBLIA_MAIN_BACKEND_ACCESS_H
#define BIBLIA_MAIN_BACKEND_ACCESS_H
#include "backend/bible_backend.h"
// Non-owning module routing: SQLite Bibles, SWORD compatibility resources.
BibleBackend &main_backend_for(const char *module);
inline BibleBackend &main_backend_for(const std::string &module) { return main_backend_for(module.c_str()); }
// The SQLite backend opened while validating the startup selection, until the
// reader's backend takes it over; nullptr otherwise.
BibleBackend *main_startup_sqlite_backend(void);
#endif
