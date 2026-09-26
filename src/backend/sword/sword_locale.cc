/* SWORD's locale manager for the reader's language only (see .h). */
#include "backend/sword/sword_locale.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <localemgr.h>
#include <stringmgr.h>
#include <swlocale.h>
#include <swmgr.h>

using namespace sword;

namespace {

/* The directories SWORD reads locales from: its install's, the
 * configuration prefix's and any augment path's, in that order. */
std::vector<std::string> localeDirectories()
{
	std::vector<std::string> directories;
	auto add = [&](std::string path) {
		if (path.empty()) return;
		if (path.back() != '/') path += '/';
		path += "locales.d";
		for (const std::string &known : directories)
			if (known == path) return;
		if (g_file_test(path.c_str(), G_FILE_TEST_IS_DIR))
			directories.push_back(path);
	};
#ifdef SWORD_INSTALL_PREFIX
	add(std::string(SWORD_INSTALL_PREFIX) + "/share/sword");
#endif
	char type = 0;
	char *prefix = nullptr, *config = nullptr;
	StringList augment;
	SWMgr::findConfig(&type, &prefix, &config, &augment);
	if (prefix) add(prefix);
	for (const SWBuf &path : augment) add(path.c_str());
	delete[] prefix;
	delete[] config;
	return directories;
}

/* A locale file's [Meta] Name, read without parsing the whole file. */
std::string localeName(const std::string &path)
{
	gchar *contents = nullptr;
	if (!g_file_get_contents(path.c_str(), &contents, nullptr, nullptr))
		return std::string();
	std::string name;
	bool inMeta = false;
	gchar **lines = g_strsplit(contents, "\n", -1);
	for (gchar **line = lines; *line; ++line) {
		g_strstrip(*line);
		if ((*line)[0] == '[') {
			if (inMeta) break;
			inMeta = !strcmp(*line, "[Meta]");
		} else if (inMeta && g_str_has_prefix(*line, "Name=")) {
			name = *line + 5;
			break;
		}
	}
	g_strfreev(lines);
	g_free(contents);
	return name;
}

class ReaderLocaleMgr : public LocaleMgr
{
public:
	/* A path that holds no locale: nothing is read at construction. */
	explicit ReaderLocaleMgr(const char *empty) : LocaleMgr(empty) {}

	/* As LocaleMgr::loadConfigDir() does for each file it reads. */
	void addFile(const std::string &path)
	{
		SWLocale *locale = new SWLocale(path.c_str());
		if (!locale->getName()) {
			delete locale;
			return;
		}
		const char *encoding = locale->getEncoding();
		const bool supported = StringMgr::hasUTF8Support()
			? (encoding && (!strcmp(encoding, "UTF-8") || !strcmp(encoding, "ASCII")))
			: (!encoding || strcmp(encoding, "UTF-8") != 0);
		if (!supported) {
			delete locale;
			return;
		}
		LocaleMap::iterator it = locales->find(locale->getName());
		if (it != locales->end()) {
			*(it->second) += *locale;
			delete locale;
		} else {
			locales->insert(LocaleMap::value_type(locale->getName(), locale));
		}
	}
};

} // namespace

char *swordInstallReaderLocale(const char *sys_locale)
{
	if (!sys_locale || !*sys_locale)
		return nullptr;
	/* name -> its files, in name order, as SWORD's map lists them. */
	std::map<std::string, std::vector<std::string>> files;
	for (const std::string &directory : localeDirectories()) {
		GDir *dir = g_dir_open(directory.c_str(), 0, nullptr);
		if (!dir) continue;
		std::vector<std::string> names;
		for (const gchar *entry; (entry = g_dir_read_name(dir));)
			if (g_str_has_suffix(entry, ".conf")) names.push_back(entry);
		g_dir_close(dir);
		std::sort(names.begin(), names.end());
		for (const std::string &entry : names) {
			const std::string path = directory + "/" + entry;
			const std::string name = localeName(path);
			if (!name.empty()) files[name].push_back(path);
		}
	}
	const int lengths[3] = {100, 5, 2};
	for (int length : lengths) {
		for (const auto &candidate : files) {
			if (strncmp(sys_locale, candidate.first.c_str(), length))
				continue;
			gchar *empty = g_dir_make_tmp("biblia-elim-locale-XXXXXX", nullptr);
			if (!empty) return nullptr;
			ReaderLocaleMgr *manager = new ReaderLocaleMgr(empty);
			g_rmdir(empty);
			g_free(empty);
			for (const std::string &path : candidate.second)
				manager->addFile(path);
			if (!manager->getLocale(candidate.first.c_str())) {
				/* every file of it was an encoding this StringMgr
				 * cannot use: SWORD would not list it either */
				delete manager;
				continue;
			}
			LocaleMgr::setSystemLocaleMgr(manager);
			manager->setDefaultLocaleName(candidate.first.c_str());
			return g_strdup(candidate.first.c_str());
		}
	}
	return nullptr;
}
