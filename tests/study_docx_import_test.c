#include <glib.h>
#include <glib/gstdio.h>
#include <minizip/zip.h>
#include <string.h>

#include "editor/docx_import.h"

static void
write_docx(const gchar *path, const gchar *document)
{
	zipFile zip = zipOpen64(path, APPEND_STATUS_CREATE);
	g_assert_nonnull(zip);
	zip_fileinfo info = { 0 };
	g_assert_cmpint(zipOpenNewFileInZip(zip, "word/document.xml", &info,
		NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION), ==, ZIP_OK);
	g_assert_cmpint(zipWriteInFileInZip(zip, document, strlen(document)), ==, ZIP_OK);
	g_assert_cmpint(zipCloseFileInZip(zip), ==, ZIP_OK);
	g_assert_cmpint(zipClose(zip, NULL), ==, ZIP_OK);
}

static void
test_import_basic_word_formatting(void)
{
	GError *error = NULL;
	gchar *path = g_build_filename(g_get_tmp_dir(), "biblia-elim-study-import.docx", NULL);
	const gchar *document =
		"<?xml version=\"1.0\"?><w:document xmlns:w=\"urn:word\"><w:body>"
		"<w:p><w:pPr><w:pStyle w:val=\"Heading1\"/></w:pPr><w:r><w:t>Gracia &amp; paz</w:t></w:r></w:p>"
		"<w:p><w:r><w:rPr><w:b/><w:i/></w:rPr><w:t>Texto</w:t></w:r><w:r><w:tab/><w:t>final</w:t></w:r></w:p>"
		"<w:p><w:pPr><w:numPr/></w:pPr><w:r><w:t>Oración</w:t></w:r></w:p>"
		"</w:body></w:document>";
	write_docx(path, document);
	gchar *html = study_docx_to_html(path, &error);
	g_assert_no_error(error);
	g_assert_nonnull(html);
	g_assert_cmpstr(html, ==,
		"<h1>Gracia &amp; paz</h1><b><i>Texto</i></b>\tfinal<br/><ul><li>Oración</li></ul>");
	g_free(html);
	g_remove(path);
	g_free(path);
}

static void
test_reject_non_docx(void)
{
	GError *error = NULL;
	gchar *path = g_build_filename(g_get_tmp_dir(), "biblia-elim-not-a-docx.docx", NULL);
	g_file_set_contents(path, "not a zip", -1, NULL);
	g_assert_null(study_docx_to_html(path, &error));
	g_assert_error(error, g_quark_from_static_string("study-docx-import"), 2);
	g_clear_error(&error);
	g_remove(path);
	g_free(path);
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/study/docx/basic-formatting", test_import_basic_word_formatting);
	g_test_add_func("/study/docx/reject-invalid", test_reject_non_docx);
	return g_test_run();
}
