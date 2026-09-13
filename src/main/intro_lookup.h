#ifndef XIPHOS_INTRO_LOOKUP_H
#define XIPHOS_INTRO_LOOKUP_H

/*
 * A verse-0 intro lookup that resolved onto verse N>0 captured a real
 * verse body (SWORD skipConsecutiveLinks / AutoNormalize).  That body
 * is not chapter intro.
 */
static inline int
intro_lookup_stole_verse_body(int requested_verse, int landed_verse)
{
	return requested_verse == 0 && landed_verse > 0;
}

#endif /* XIPHOS_INTRO_LOOKUP_H */
