#pragma once

#include <glib.h>
#include <swmgr.h>

// Read-only package fallback; existing user modules always take precedence.
const char *bundledDevotionalId(const char *language);
void loadBundledDevotional(sword::SWMgr &manager, const char *language);
void loadBundledDevotionalFrom(sword::SWMgr &manager, const char *directory,
                              const char *language);

// Keep an external preference; bundled defaults follow the interface language.
const char *selectDevotional(GList *installed, const char *saved,
                            const char *language);
