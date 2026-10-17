#ifndef STUDY_TEMPLATES_H
#define STUDY_TEMPLATES_H

#include <glib.h>

typedef struct {
	const gchar *name;
	const gchar *description;
	const gchar *html;
} StudyTemplate;

guint study_template_count(void);
const StudyTemplate *study_template_get(guint index);

#endif
