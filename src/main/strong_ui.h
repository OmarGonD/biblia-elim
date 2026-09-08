#ifndef XIPHOS_STRONG_UI_H
#define XIPHOS_STRONG_UI_H

#include <stddef.h>

#ifdef __cplusplus
class BibleLexicon;
extern "C" {
#endif

void main_show_neutral_strong(const char *module, const char *passage,
	size_t byte_offset);
void main_show_neutral_footnote(const char *module, const char *passage,
	size_t sequence);
void main_show_neutral_crossref(const char *module, const char *passage,
	size_t sequence);

#ifdef __cplusplus
void main_set_strong_lexicon(BibleLexicon *lexicon);
}
#endif

#endif /* XIPHOS_STRONG_UI_H */
