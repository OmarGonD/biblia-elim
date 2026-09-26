/* SWORD implementation of the neutral BibleBackend API. */
#ifndef XIPHOS_SWORD_BACKEND_H
#define XIPHOS_SWORD_BACKEND_H

#include "backend/sword_main.hh"
#include "backend/versification_mapper.h"

#include <memory>

class SwordBackend final : public BackEnd
{
public:
	SwordBackend() = default;
	~SwordBackend() override = default;
};

/* libsword's versification tables as a VersificationMapper: the same
 * mapping SWORD modules convert references with (swordMapVerseKey), for
 * backends that store verses without them. No module is needed. */
std::shared_ptr<const VersificationMapper> makeSwordVersificationMapper();

#endif /* XIPHOS_SWORD_BACKEND_H */
