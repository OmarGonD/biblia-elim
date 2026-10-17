#ifndef XIPHOS_STUDY_DOCUMENT_EXPORT_H
#define XIPHOS_STUDY_DOCUMENT_EXPORT_H

#include <glib.h>

G_BEGIN_DECLS

gboolean study_document_export_docx(const gchar *html, const gchar *path,
				    GError **error);
gboolean study_document_export_pdf(const gchar *html, const gchar *path,
				   GError **error);

G_END_DECLS
#endif
