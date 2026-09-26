#ifndef BIBLIA_SQLITE_MODULE_MANAGER_DIALOG_H
#define BIBLIA_SQLITE_MODULE_MANAGER_DIALOG_H
#include <glib.h>
G_BEGIN_DECLS
void gui_open_sqlite_module_manager(void);
void gui_offer_sword_conversion(const char *module_id);
/* Converts, without asking and in the background (progress bar in the status
 * bar), every installed SWORD Bible that has no SQLite copy yet (and whose
 * conversion has not failed before), then switches the reader to SQLite.
 * Does nothing when SWORD was chosen explicitly. */
void gui_convert_pending_sword_bibles(void);
/* After a SWORD Bible is uninstalled: removes the SQLite copy converted
 * from it (never a module installed or imported by other means). */
void gui_forget_converted_sword_bible(const char *module_id);
/* On quit: lets the Bible being converted finish, drops the rest. */
void gui_stop_sword_conversion(void);
/* `biblia-elim --convert-sword ID DIR`: the child process of the background
 * conversion. Returns the exit status. */
int sword_conversion_child_main(const char *id, const char *directory);
G_END_DECLS
#endif
