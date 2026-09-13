#include "backend/source_quirks.h"
#include "backend/quoted_heading.h"

namespace {

struct ModuleQuirk {
	const char *module_id;
	unsigned quirks;
};

/* Known source anomalies only. Add a row when a new external module
 * encodes the same defect; do not infer from punctuation at read time
 * for unlisted modules. */
const ModuleQuirk kModuleQuirks[] = {
	{ "SpaRVG", SOURCE_QUIRK_LEADING_QUOTED_SUPERSCRIPTION },
};

} // namespace

unsigned
sourceQuirksForModule(const std::string &module_id)
{
	for (unsigned i = 0; i < sizeof(kModuleQuirks) / sizeof(kModuleQuirks[0]);
	     i++) {
		if (module_id == kModuleQuirks[i].module_id)
			return kModuleQuirks[i].quirks;
	}
	return SOURCE_QUIRK_NONE;
}

void
applySourceQuirks(const std::string &module_id, BibleVerseContent &content)
{
	const unsigned quirks = sourceQuirksForModule(module_id);
	if (quirks & SOURCE_QUIRK_LEADING_QUOTED_SUPERSCRIPTION)
		promoteLeadingQuotedHeading(content);
}
