/* Safe, small DOCX reader for StudyPad.  It reads only word/document.xml. */
#include "editor/docx_import.h"

#include <minizip/unzip.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define DOCX_MAX_DOCUMENT_XML (8 * 1024 * 1024)

static GQuark
docx_error_quark(void)
{
	return g_quark_from_static_string("study-docx-import");
}

static gboolean
is(xmlNodePtr node, const char *name)
{
	return node && node->type == XML_ELEMENT_NODE &&
	       !xmlStrcmp(node->name, (const xmlChar *)name);
}

static xmlNodePtr
child(xmlNodePtr node, const char *name)
{
	for (xmlNodePtr n = node ? node->children : NULL; n; n = n->next)
		if (is(n, name)) return n;
	return NULL;
}

static void
append_text(GString *out, const xmlChar *text)
{
	gchar *escaped = g_markup_escape_text((const gchar *)text, -1);
	g_string_append(out, escaped);
	g_free(escaped);
}

static void
append_run(GString *out, xmlNodePtr run)
{
	xmlNodePtr props = child(run, "rPr");
	gboolean bold = child(props, "b") != NULL;
	gboolean italic = child(props, "i") != NULL;
	gboolean underline = child(props, "u") != NULL;
	gboolean strike = child(props, "strike") != NULL;
	if (bold) g_string_append(out, "<b>");
	if (italic) g_string_append(out, "<i>");
	if (underline) g_string_append(out, "<u>");
	if (strike) g_string_append(out, "<s>");
	for (xmlNodePtr n = run->children; n; n = n->next) {
		if (is(n, "t") && n->children)
			append_text(out, n->children->content);
		else if (is(n, "tab"))
			g_string_append(out, "\t");
		else if (is(n, "br") || is(n, "cr"))
			g_string_append(out, "<br/>");
	}
	if (strike) g_string_append(out, "</s>");
	if (underline) g_string_append(out, "</u>");
	if (italic) g_string_append(out, "</i>");
	if (bold) g_string_append(out, "</b>");
}

static void
append_paragraph(GString *out, xmlNodePtr paragraph)
{
	xmlNodePtr props = child(paragraph, "pPr");
	xmlNodePtr style = child(props, "pStyle");
	xmlChar *style_name = style ? xmlGetProp(style, (const xmlChar *)"val") : NULL;
	const char *tag = NULL;
	if (style_name && g_str_has_prefix((const gchar *)style_name, "Heading")) {
		gint level = g_ascii_digit_value(style_name[7]);
		if (level >= 1 && level <= 6) tag = level == 1 ? "h1" :
			level == 2 ? "h2" : level == 3 ? "h3" : level == 4 ? "h4" :
			level == 5 ? "h5" : "h6";
	}
	gboolean list = child(props, "numPr") != NULL;
	if (tag) g_string_append_printf(out, "<%s>", tag);
	else if (list) g_string_append(out, "<ul><li>");
	for (xmlNodePtr n = paragraph->children; n; n = n->next) {
		if (is(n, "r")) append_run(out, n);
		else if (is(n, "hyperlink"))
			for (xmlNodePtr r = n->children; r; r = r->next)
				if (is(r, "r")) append_run(out, r);
	}
	if (tag) g_string_append_printf(out, "</%s>", tag);
	else if (list) g_string_append(out, "</li></ul>");
	else g_string_append(out, "<br/>");
	if (style_name) xmlFree(style_name);
}

gchar *
study_docx_to_html(const gchar *filename, GError **error)
{
	unzFile zip;
	unz_file_info64 info;
	gchar *xml = NULL;
	gchar *html = NULL;

	if (!filename || !*filename) {
		g_set_error_literal(error, docx_error_quark(), 1, "No se indicó un archivo DOCX.");
		return NULL;
	}
	zip = unzOpen64(filename);
	if (!zip || unzLocateFile(zip, "word/document.xml", 0) != UNZ_OK ||
	    unzGetCurrentFileInfo64(zip, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK ||
	    info.uncompressed_size > DOCX_MAX_DOCUMENT_XML ||
	    unzOpenCurrentFile(zip) != UNZ_OK) {
		if (zip) unzClose(zip);
		g_set_error_literal(error, docx_error_quark(), 2,
			"El archivo no es un documento Word DOCX válido o es demasiado grande.");
		return NULL;
	}
	xml = g_malloc(info.uncompressed_size + 1);
	int read = unzReadCurrentFile(zip, xml, (unsigned)info.uncompressed_size);
	unzCloseCurrentFile(zip);
	unzClose(zip);
	if (read < 0 || (guint64)read != info.uncompressed_size) {
		g_free(xml);
		g_set_error_literal(error, docx_error_quark(), 3, "No se pudo leer el contenido del documento DOCX.");
		return NULL;
	}
	xml[read] = '\0';
	xmlDocPtr doc = xmlReadMemory(xml, read, filename, "UTF-8",
		XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
	g_free(xml);
	if (!doc) {
		g_set_error_literal(error, docx_error_quark(), 4, "El contenido XML del DOCX no es válido.");
		return NULL;
	}
	xmlNodePtr body = NULL;
	for (xmlNodePtr n = xmlDocGetRootElement(doc); n && !body; n = n->children) {
		if (is(n, "body")) body = n;
		for (xmlNodePtr c = n->children; c && !body; c = c->next)
			if (is(c, "body")) body = c;
	}
	GString *out = g_string_new("");
	for (xmlNodePtr n = body ? body->children : NULL; n; n = n->next)
		if (is(n, "p")) append_paragraph(out, n);
	xmlFreeDoc(doc);
	html = g_string_free(out, FALSE);
	return html;
}
