/* El aviso por el defecto del módulo Tisch aparece solo en Jn 8:12-53. */
#include <glib.h>
#include <string.h>

#include "main/interlineal.h"

int
main(void)
{
	char ref[32];

	g_assert_null(main_interlineal_aviso_tisch(NULL));
	g_assert_null(main_interlineal_aviso_tisch("John.1.1"));
	g_assert_null(main_interlineal_aviso_tisch("John.8.11"));
	g_assert_null(main_interlineal_aviso_tisch("John.8.54"));
	g_assert_null(main_interlineal_aviso_tisch("Matt.8.20"));
	g_assert_null(main_interlineal_aviso_tisch("Luke.8.20"));
	for (int v = 12; v <= 52; v++) {
		g_snprintf(ref, sizeof ref, "John.8.%d", v);
		const char *a = main_interlineal_aviso_tisch(ref);
		g_assert_nonnull(a);
		g_assert_nonnull(strstr(a, "no está disponible"));
	}
	const char *a = main_interlineal_aviso_tisch("John.8.53");
	g_assert_nonnull(a);
	g_assert_nonnull(strstr(a, "incompleto"));
	return 0;
}
