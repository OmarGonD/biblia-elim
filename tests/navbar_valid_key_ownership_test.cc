#include <gtk/gtk.h>

#include "backend/bible_backend.h"
#include "fake_bible_backend.h"
#include "main/navbar_versekey.h"

#include <cstring>
#include <iostream>

BibleBackend *bible_backend;

namespace {
int failures;

#define CHECK(condition) do { \
	if (!(condition)) { \
		std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ \
			<< ": " #condition "\n"; \
		++failures; \
	} \
} while (0)
}

int main()
{
	FakeBibleBackend backend;
	bible_backend = &backend;

	gchar *genesis = main_get_valid_key("FakeBible", "Genesis 1:1");
	gchar *john = main_get_valid_key("FakeBible", "John 3:16");
	gchar *next = main_get_valid_key("FakeBible", "John 3:17");

	CHECK(genesis != nullptr);
	CHECK(john != nullptr);
	CHECK(next != nullptr);
	CHECK(genesis != john && john != next && genesis != next);
	CHECK(genesis && std::strcmp(genesis, "Genesis 1:1") == 0);
	CHECK(john && std::strcmp(john, "John 3:16") == 0);
	CHECK(next && std::strcmp(next, "John 3:17") == 0);
	CHECK(main_get_valid_key("FakeBible", "invalid") == nullptr);
	for (int iteration = 0; iteration < 1000; ++iteration) {
		const char *expected = iteration % 2 ? "John 3:16" : "Genesis 1:1";
		gchar *key = main_get_valid_key("FakeBible", expected);
		CHECK(key && std::strcmp(key, expected) == 0);
		g_free(key);
	}

	/* Each result remains independent after later calls and may be released
	 * through the documented GLib allocator contract in any order. */
	g_free(john);
	CHECK(genesis && std::strcmp(genesis, "Genesis 1:1") == 0);
	CHECK(next && std::strcmp(next, "John 3:17") == 0);
	g_free(genesis);
	g_free(next);

	std::cout << "navbar_valid_key_ownership_failures=" << failures << '\n';
	return failures ? 1 : 0;
}
