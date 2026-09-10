#include "main/startup_diagnostics.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <cstdio>
#include <string>

namespace {

int failures;

#define CHECK(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
				     #condition);                                    \
			failures++;                                               \
		}                                                                 \
	} while (0)

} // namespace

int main()
{
	GError *error = nullptr;
	gchar *temporary = g_dir_make_tmp("biblia-elim-startup-diagnostics-XXXXXX",
					 &error);
	CHECK(temporary != nullptr);
	if (!temporary) {
		std::fprintf(stderr, "temporary directory: %s\n",
			     error ? error->message : "unknown error");
		g_clear_error(&error);
		return 1;
	}

	const std::string directory(temporary);
	const std::string missing = directory + "/missing";
	const std::string ordinary_file = directory + "/README";
	const std::string invalid_module = directory + "/invalid.sqlite";

	CHECK(sqliteFallbackDiagnosticLevel(missing, false) ==
	      StartupDiagnosticLevel::Message);
	CHECK(sqliteFallbackDiagnosticLevel(directory, false) ==
	      StartupDiagnosticLevel::Message);
	CHECK(g_file_set_contents(ordinary_file.c_str(), "not a module", -1, &error));
	CHECK(sqliteFallbackDiagnosticLevel(directory, false) ==
	      StartupDiagnosticLevel::Message);
	CHECK(g_file_set_contents(invalid_module.c_str(), "not sqlite", -1, &error));
	CHECK(sqliteFallbackDiagnosticLevel(directory, false) ==
	      StartupDiagnosticLevel::Warning);

	/* A CLI choice or environment override is configuration, so failure stays
	 * a warning even when the configured directory does not exist. */
	CHECK(sqliteFallbackDiagnosticLevel(missing, true) ==
	      StartupDiagnosticLevel::Warning);
	CHECK(sqliteFallbackDiagnosticLevel(ordinary_file, false) ==
	      StartupDiagnosticLevel::Warning);

	g_remove(invalid_module.c_str());
	g_remove(ordinary_file.c_str());
	g_rmdir(directory.c_str());
	g_free(temporary);
	g_clear_error(&error);

	std::printf("startup_diagnostics_failures=%d\n", failures);
	return failures ? 1 : 0;
}
