#include "main/startup_diagnostics.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>

namespace {

bool hasSuffix(const char *value, const char *suffix)
{
	const std::size_t value_length = std::strlen(value);
	const std::size_t suffix_length = std::strlen(suffix);
	return value_length >= suffix_length &&
	       std::strcmp(value + value_length - suffix_length, suffix) == 0;
}

} // namespace

StartupDiagnosticLevel sqliteFallbackDiagnosticLevel(
	const std::string &modules_directory, bool explicitly_configured)
{
	if (explicitly_configured)
		return StartupDiagnosticLevel::Warning;

	errno = 0;
	DIR *directory = opendir(modules_directory.c_str());
	if (!directory)
		return errno == ENOENT ? StartupDiagnosticLevel::Message
				       : StartupDiagnosticLevel::Warning;

	StartupDiagnosticLevel level = StartupDiagnosticLevel::Message;
	while (dirent *entry = readdir(directory)) {
		if (hasSuffix(entry->d_name, ".sqlite")) {
			/* The backend rejected at least one apparent module.  Keep this
			 * visible because it can indicate an invalid or corrupt file. */
			level = StartupDiagnosticLevel::Warning;
			break;
		}
	}
	closedir(directory);
	return level;
}
