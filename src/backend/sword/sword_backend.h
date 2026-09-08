/* SWORD implementation of the neutral BibleBackend API. */
#ifndef XIPHOS_SWORD_BACKEND_H
#define XIPHOS_SWORD_BACKEND_H

#include "backend/sword_main.hh"

class SwordBackend final : public BackEnd
{
public:
	SwordBackend() = default;
	~SwordBackend() override = default;
};

#endif /* XIPHOS_SWORD_BACKEND_H */
