/*
 * Biblia Elim
 * nube_mayusculas.h - palabras del texto y cómo las escribe (CLOUD-CASE-101)
 *
 * La nube cuenta las palabras en minúsculas («noemí» y «Noemí» son la
 * misma), pero tiene que enseñarlas como las escribe la Biblia: los
 * nombres propios con mayúscula. Una lista fija de nombres siempre deja
 * fuera alguno (Booz, Noemí, Moab, Elimelec...), así que se aprende del
 * propio texto: una palabra que dentro de la frase va con mayúscula más
 * veces que en minúscula es un nombre propio. Al principio de frase o de
 * versículo la mayúscula no dice nada y no cuenta.
 *
 * No sabe de Sword: recibe texto. Por eso se prueba sola.
 */
#ifndef BIBLIA_ELIM_NUBE_MAYUSCULAS_H
#define BIBLIA_ELIM_NUBE_MAYUSCULAS_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Cada palabra de `texto` (un versículo), tal como está escrita, y si
 * abre frase: al principio del texto, tras «.», «!», «?», «:», «¿», «¡»,
 * o en mayúsculas entre las dos primeras del versículo (la entrada en
 * versalitas de un capítulo, «Y ACONTECIÓ»). Ahí la mayúscula no dice si
 * es un nombre. Una palabra son letras seguidas, con apóstrofos dentro. */
typedef void (*NubePalabraVista)(const gchar *palabra, gboolean abre_frase,
				 gpointer datos);
void nube_recorrer_palabras(const gchar *texto, NubePalabraVista cb,
			    gpointer datos);

/* Lo visto de cada palabra: minúsculas -> cómo se escribe. */
typedef struct _NubeMayusculas NubeMayusculas;
NubeMayusculas *nube_mayusculas_nueva(void);
void nube_mayusculas_libre(NubeMayusculas *m);
/* Anota una aparición de `palabra`, escrita así. */
void nube_mayusculas_anotar(NubeMayusculas *m, const gchar *palabra,
			    gboolean abre_frase);
/* Cómo se enseña `minusculas`: con la inicial en mayúscula si el texto la
 * trata como nombre propio, si no tal cual. El llamador libera. */
gchar *nube_mayusculas_forma(NubeMayusculas *m, const gchar *minusculas);

#ifdef __cplusplus
}
#endif

#endif /* BIBLIA_ELIM_NUBE_MAYUSCULAS_H */
