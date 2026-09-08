#include <glib.h>

#include "bible_backend_contract.h"
#include "fake_bible_backend.h"
#include "strong_backend_contract.h"

static void fake_backend_obeys_common_contract()
{
	FakeBibleBackend backend;
	runBibleBackendContractTests(backend);
}

static void fake_backend_obeys_strong_contract()
{
	FakeBibleBackend backend;
	runStrongBackendContractTests(backend);
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/backend/contract/fake",
			fake_backend_obeys_common_contract);
	g_test_add_func("/backend/contract/strong/fake",
			fake_backend_obeys_strong_contract);
	return g_test_run();
}
