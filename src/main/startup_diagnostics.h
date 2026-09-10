#ifndef BIBLIA_ELIM_STARTUP_DIAGNOSTICS_H
#define BIBLIA_ELIM_STARTUP_DIAGNOSTICS_H

#include <string>

enum class StartupDiagnosticLevel {
	Message,
	Warning
};

StartupDiagnosticLevel sqliteFallbackDiagnosticLevel(
	const std::string &modules_directory, bool explicitly_configured);

#endif
