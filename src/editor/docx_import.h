/* Import the readable subset of an OOXML Word document into StudyPad HTML. */
#ifndef XIPHOS_DOCX_IMPORT_H
#define XIPHOS_DOCX_IMPORT_H

#include <glib.h>

G_BEGIN_DECLS

/* The returned HTML belongs to the caller.  DOCX is intentionally the
 * supported Word format: legacy .doc is a proprietary binary format. */
gchar *study_docx_to_html(const gchar *filename, GError **error);

G_END_DECLS

#endif
