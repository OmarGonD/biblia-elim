#include "editor/study_document_export.h"

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <cairo-pdf.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <minizip/zip.h>

typedef struct {
	gboolean bold;
	gboolean italic;
	gboolean underline;
	gboolean strike;
	gboolean heading;
	gchar *color;
	gchar *link;
} InlineStyle;

typedef struct {
	gchar *id;
	gchar *uri;
} DocxRelationship;

static GQuark
export_error_quark(void)
{
	return g_quark_from_static_string("study-document-export");
}

static xmlNodePtr
find_body(xmlDocPtr doc)
{
	xmlNodePtr root = xmlDocGetRootElement(doc);
	for (xmlNodePtr node = root; node; node = node->next) {
		if (node->type == XML_ELEMENT_NODE &&
		    !xmlStrcasecmp(node->name, (const xmlChar *)"body"))
			return node;
		for (xmlNodePtr child = node->children; child; child = child->next)
			if (child->type == XML_ELEMENT_NODE &&
			    !xmlStrcasecmp(child->name, (const xmlChar *)"body"))
				return child;
	}
	return NULL;
}

static gchar *
escape_xml(const gchar *text)
{
	return g_markup_escape_text(text ? text : "", -1);
}

static void
style_copy(InlineStyle *dest, const InlineStyle *source)
{
	*dest = *source;
	dest->color = g_strdup(source->color);
	dest->link = g_strdup(source->link);
}

static void
style_clear(InlineStyle *style)
{
	g_free(style->color);
	g_free(style->link);
}

static gchar *
relationship_id(GPtrArray *relationships, const gchar *uri)
{
	for (guint i = 0; i < relationships->len; i++) {
		DocxRelationship *rel = g_ptr_array_index(relationships, i);
		if (g_strcmp0(rel->uri, uri) == 0)
			return g_strdup(rel->id);
	}
	DocxRelationship *rel = g_new0(DocxRelationship, 1);
	rel->id = g_strdup_printf("rId%u", relationships->len + 1);
	rel->uri = g_strdup(uri);
	g_ptr_array_add(relationships, rel);
	return g_strdup(rel->id);
}

static void
relationship_free(gpointer data)
{
	DocxRelationship *rel = data;
	g_free(rel->id);
	g_free(rel->uri);
	g_free(rel);
}

static void
append_docx_run(GString *out, const gchar *text, const InlineStyle *style,
		GPtrArray *relationships)
{
	gchar *escaped = escape_xml(text);
	gchar *rid = style->link ? relationship_id(relationships, style->link) : NULL;
	if (rid)
		g_string_append_printf(out, "<w:hyperlink r:id=\"%s\">", rid);
	g_string_append(out, "<w:r>");
	if (style->bold || style->italic || style->underline || style->strike ||
	    style->heading || style->color || style->link) {
		g_string_append(out, "<w:rPr>");
		if (style->bold || style->heading) g_string_append(out, "<w:b/>");
		if (style->italic) g_string_append(out, "<w:i/>");
		if (style->underline || style->link)
			g_string_append(out, "<w:u w:val=\"single\"/>");
		if (style->strike) g_string_append(out, "<w:strike/>");
		if (style->heading) g_string_append(out, "<w:sz w:val=\"28\"/>");
		if (style->color) {
			GdkRGBA color;
			if (gdk_rgba_parse(&color, style->color)) {
				gchar hex[7];
				g_snprintf(hex, sizeof(hex), "%02X%02X%02X",
					   (guint)(color.red * 255 + 0.5),
					   (guint)(color.green * 255 + 0.5),
					   (guint)(color.blue * 255 + 0.5));
				g_string_append_printf(out, "<w:color w:val=\"%s\"/>", hex);
			}
		}
		g_string_append(out, "</w:rPr>");
	}
	g_string_append_printf(out,
		"<w:t xml:space=\"preserve\">%s</w:t></w:r>", escaped);
	if (rid)
		g_string_append(out, "</w:hyperlink>");
	g_free(rid);
	g_free(escaped);
}

static void
append_docx_nodes(GString *out, xmlNodePtr nodes, const InlineStyle *parent,
		  GPtrArray *relationships)
{
	for (xmlNodePtr node = nodes; node; node = node->next) {
		if (node->type == XML_TEXT_NODE) {
			if (node->content && *node->content)
				append_docx_run(out, (const gchar *)node->content,
						parent, relationships);
			continue;
		}
		if (node->type != XML_ELEMENT_NODE)
			continue;
		InlineStyle style;
		style_copy(&style, parent);
		const gchar *name = (const gchar *)node->name;
		if (!g_ascii_strcasecmp(name, "b") || !g_ascii_strcasecmp(name, "strong"))
			style.bold = TRUE;
		else if (!g_ascii_strcasecmp(name, "i") || !g_ascii_strcasecmp(name, "em"))
			style.italic = TRUE;
		else if (!g_ascii_strcasecmp(name, "u"))
			style.underline = TRUE;
		else if (!g_ascii_strcasecmp(name, "s") || !g_ascii_strcasecmp(name, "strike"))
			style.strike = TRUE;
		else if (g_ascii_strcasecmp(name, "h1") == 0 ||
			g_ascii_strcasecmp(name, "h2") == 0 ||
			g_ascii_strcasecmp(name, "h3") == 0)
			style.heading = TRUE;
		else if (!g_ascii_strcasecmp(name, "span")) {
			xmlChar *attribute = xmlGetProp(node, (const xmlChar *)"style");
			if (attribute) {
				const gchar *value = (const gchar *)attribute;
				if (g_str_has_prefix(value, "color:")) {
					g_free(style.color);
					style.color = g_strdup(value + 6);
				}
				xmlFree(attribute);
			}
		} else if (!g_ascii_strcasecmp(name, "a")) {
			xmlChar *href = xmlGetProp(node, (const xmlChar *)"href");
			if (href) {
				g_free(style.link);
				style.link = g_strdup((const gchar *)href);
				xmlFree(href);
			}
		} else if (!g_ascii_strcasecmp(name, "br")) {
			g_string_append(out, "<w:r><w:br/></w:r>");
			style_clear(&style);
			continue;
		} else if (!g_ascii_strcasecmp(name, "li")) {
			append_docx_run(out, "• ", parent, relationships);
		}
		append_docx_nodes(out, node->children, &style, relationships);
		style_clear(&style);
	}
}

static gboolean
zip_add(zipFile archive, const gchar *name, const gchar *data)
{
	zip_fileinfo info = { 0 };
	if (zipOpenNewFileInZip64(archive, name, &info, NULL, 0, NULL, 0, NULL,
				  Z_DEFLATED, Z_DEFAULT_COMPRESSION, 1) != ZIP_OK)
		return FALSE;
	gsize length = strlen(data);
	int result = length > G_MAXUINT ? ZIP_ERRNO :
		zipWriteInFileInZip(archive, data, (unsigned int)length);
	int close_result = zipCloseFileInZip(archive);
	return result == ZIP_OK && close_result == ZIP_OK;
}

gboolean
study_document_export_docx(const gchar *html, const gchar *path, GError **error)
{
	static const gchar content_types[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
		"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
		"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
		"<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
		"<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
		"</Types>";
	static const gchar package_rels[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
		"</Relationships>";
	GError *local_error = NULL;
	gchar *wrapped = g_strdup_printf("<html><body>%s</body></html>", html ? html : "");
	xmlDocPtr doc = htmlReadMemory(wrapped, strlen(wrapped), path, "UTF-8",
				       HTML_PARSE_RECOVER | HTML_PARSE_NONET |
				       HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	g_free(wrapped);
	if (!doc) {
		g_set_error_literal(error, export_error_quark(), 1,
				    "No se pudo interpretar el contenido del estudio.");
		return FALSE;
	}
	GString *document = g_string_new(
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
		"<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><w:body><w:p>");
	GPtrArray *relationships = g_ptr_array_new_with_free_func(relationship_free);
	InlineStyle initial = { 0 };
	append_docx_nodes(document, find_body(doc) ? find_body(doc)->children : NULL,
			  &initial, relationships);
	g_string_append(document,
		"</w:p><w:sectPr><w:pgSz w:w=\"12240\" w:h=\"15840\"/>"
		"<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\"/></w:sectPr></w:body></w:document>");
	xmlFreeDoc(doc);
	GString *rels = g_string_new(
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
	for (guint i = 0; i < relationships->len; i++) {
		DocxRelationship *rel = g_ptr_array_index(relationships, i);
		gchar *uri = escape_xml(rel->uri);
		g_string_append_printf(rels,
			"<Relationship Id=\"%s\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" Target=\"%s\" TargetMode=\"External\"/>",
			rel->id, uri);
		g_free(uri);
	}
	g_string_append(rels, "</Relationships>");
	zipFile archive = zipOpen64(path, APPEND_STATUS_CREATE);
	gboolean ok = archive != NULL;
	if (ok) ok = zip_add(archive, "[Content_Types].xml", content_types);
	if (ok) ok = zip_add(archive, "_rels/.rels", package_rels);
	if (ok) ok = zip_add(archive, "word/document.xml", document->str);
	if (ok) ok = zip_add(archive, "word/_rels/document.xml.rels", rels->str);
	if (archive && zipClose(archive, NULL) != ZIP_OK) ok = FALSE;
	if (!ok) {
		g_set_error(&local_error, export_error_quark(), 2,
			    "No se pudo crear el documento Word en %s.", path);
		if (error) g_propagate_error(error, local_error);
		else g_clear_error(&local_error);
	}
	g_ptr_array_unref(relationships);
	g_string_free(rels, TRUE);
	g_string_free(document, TRUE);
	return ok;
}

static void
append_pango_nodes(GString *out, xmlNodePtr nodes, const gchar *foreground)
{
	for (xmlNodePtr node = nodes; node; node = node->next) {
		if (node->type == XML_TEXT_NODE) {
			gchar *escaped = g_markup_escape_text(
				(const gchar *)node->content, -1);
			g_string_append(out, escaped);
			g_free(escaped);
			continue;
		}
		if (node->type != XML_ELEMENT_NODE)
			continue;
		const gchar *name = (const gchar *)node->name;
		if (!g_ascii_strcasecmp(name, "br")) {
			g_string_append_c(out, '\n');
			continue;
		}
		if (!g_ascii_strcasecmp(name, "h1") ||
		    !g_ascii_strcasecmp(name, "h2") ||
		    !g_ascii_strcasecmp(name, "h3")) {
			gint size = !g_ascii_strcasecmp(name, "h1") ? 18000 :
				!g_ascii_strcasecmp(name, "h2") ? 15000 : 13000;
			g_string_append_printf(out, "\n<span weight=\"bold\" size=\"%d\">", size);
			append_pango_nodes(out, node->children, foreground);
			g_string_append(out, "</span>\n");
			continue;
		}
		if (!g_ascii_strcasecmp(name, "b") || !g_ascii_strcasecmp(name, "strong"))
			g_string_append(out, "<b>");
		else if (!g_ascii_strcasecmp(name, "i") || !g_ascii_strcasecmp(name, "em"))
			g_string_append(out, "<i>");
		else if (!g_ascii_strcasecmp(name, "u") || !g_ascii_strcasecmp(name, "a"))
			g_string_append(out, "<u>");
		else if (!g_ascii_strcasecmp(name, "s") || !g_ascii_strcasecmp(name, "strike"))
			g_string_append(out, "<s>");
		else if (!g_ascii_strcasecmp(name, "span")) {
			xmlChar *style = xmlGetProp(node, (const xmlChar *)"style");
			gboolean has_color = FALSE;
			GdkRGBA color;
			if (style && g_str_has_prefix((const gchar *)style, "color:")) {
				has_color = gdk_rgba_parse(&color,
							  (const gchar *)style + 6);
				if (has_color) {
					gchar hex[8];
					g_snprintf(hex, sizeof(hex), "#%02X%02X%02X",
						   (guint)(color.red * 255 + 0.5),
						   (guint)(color.green * 255 + 0.5),
						   (guint)(color.blue * 255 + 0.5));
					g_string_append_printf(out,
							       "<span foreground=\"%s\">", hex);
				}
				xmlFree(style);
				append_pango_nodes(out, node->children, foreground);
				if (has_color)
					g_string_append(out, "</span>");
				continue;
			}
			if (style) xmlFree(style);
		}
		append_pango_nodes(out, node->children, foreground);
		if (!g_ascii_strcasecmp(name, "b") || !g_ascii_strcasecmp(name, "strong"))
			g_string_append(out, "</b>");
		else if (!g_ascii_strcasecmp(name, "i") || !g_ascii_strcasecmp(name, "em"))
			g_string_append(out, "</i>");
		else if (!g_ascii_strcasecmp(name, "u") || !g_ascii_strcasecmp(name, "a"))
			g_string_append(out, "</u>");
		else if (!g_ascii_strcasecmp(name, "s") || !g_ascii_strcasecmp(name, "strike"))
			g_string_append(out, "</s>");
	}
}

static void
on_pdf_begin(GtkPrintOperation *operation, GtkPrintContext *context,
	     gpointer user_data)
{
	PangoLayout *layout = user_data;
	gint height;
	double page_height = gtk_print_context_get_height(context);
	double page_width = gtk_print_context_get_width(context);
	pango_layout_set_width(layout, (gint)(page_width * PANGO_SCALE));
	pango_layout_get_pixel_size(layout, NULL, &height);
	gtk_print_operation_set_n_pages(operation,
		MAX(1, (height + (gint)page_height - 1) / (gint)page_height));
}

static void
on_pdf_draw(GtkPrintOperation *operation, GtkPrintContext *context, gint page,
	    gpointer user_data)
{
	PangoLayout *layout = user_data;
	cairo_t *cr = gtk_print_context_get_cairo_context(context);
	double page_height = gtk_print_context_get_height(context);
	cairo_save(cr);
	cairo_rectangle(cr, 0, 0, gtk_print_context_get_width(context), page_height);
	cairo_clip(cr);
	cairo_move_to(cr, 0, -page * page_height);
	pango_cairo_show_layout(cr, layout);
	cairo_restore(cr);
}

gboolean
study_document_export_pdf(const gchar *html, const gchar *path, GError **error)
{
	gchar *wrapped = g_strdup_printf("<html><body>%s</body></html>", html ? html : "");
	xmlDocPtr doc = htmlReadMemory(wrapped, strlen(wrapped), path, "UTF-8",
				       HTML_PARSE_RECOVER | HTML_PARSE_NONET |
				       HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	g_free(wrapped);
	if (!doc) {
		g_set_error_literal(error, export_error_quark(), 3,
				    "No se pudo interpretar el contenido del estudio.");
		return FALSE;
	}
	GString *markup = g_string_new("");
	append_pango_nodes(markup, find_body(doc) ? find_body(doc)->children : NULL,
			   NULL);
	xmlFreeDoc(doc);
	GtkPrintOperation *operation = gtk_print_operation_new();
	GtkWidget *widget = gtk_text_view_new();
	g_object_ref_sink(widget);
	PangoLayout *layout = gtk_widget_create_pango_layout(widget, NULL);
	PangoFontDescription *font = pango_font_description_from_string("Serif 11");
	pango_layout_set_font_description(layout, font);
	pango_font_description_free(font);
	pango_layout_set_markup(layout, markup->str, markup->len);
	g_string_free(markup, TRUE);
	g_signal_connect(operation, "begin-print", G_CALLBACK(on_pdf_begin), layout);
	g_signal_connect(operation, "draw-page", G_CALLBACK(on_pdf_draw), layout);
	gtk_print_operation_set_export_filename(operation, path);
	GtkPrintOperationResult result = gtk_print_operation_run(
		operation, GTK_PRINT_OPERATION_ACTION_EXPORT, NULL, NULL);
	g_object_unref(layout);
	g_object_unref(widget);
	g_object_unref(operation);
	if (result != GTK_PRINT_OPERATION_RESULT_APPLY) {
		g_set_error_literal(error, export_error_quark(), 4,
				    "No se pudo exportar el PDF.");
		return FALSE;
	}
	return TRUE;
}
