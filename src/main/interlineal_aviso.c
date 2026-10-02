/*
 * Biblia Elim — aviso por defectos conocidos del módulo SWORD Tisch.
 *
 * Defecto del módulo oficial Tisch de CrossWire (ver data/sources/NOTAS_TISCH.md): Jn 8:12-52 están
 * vacíos y todo el texto de Jn 7:53-8:53 está en la entrada de Jn 8:53, de la que SWORD solo lee los
 * primeros 40 507 bytes. Se avisa al usuario sin cambiar de dónde sale el texto.
 */
#include <glib/gi18n.h>
#include <stdio.h>

#include "main/interlineal.h"

const char *
main_interlineal_aviso_tisch(const char *osis)
{
	int cap = 0, ver = 0;

	if (!osis || sscanf(osis, "John.%d.%d", &cap, &ver) != 2 || cap != 8)
		return NULL;
	if (ver >= 12 && ver <= 52)
		return _("Este versículo no está disponible en el módulo Tischendorf "
			 "(defecto del módulo oficial de CrossWire). "
			 "Lo que aparece abajo no es texto de Tischendorf.");
	if (ver == 53)
		return _("En el módulo Tischendorf este versículo aparece incompleto: "
			 "contiene mezclado el texto de Jn 7:53–8:21 "
			 "(defecto del módulo oficial de CrossWire).");
	return NULL;
}
