"""
Títulos de salmo que ocupan un versículo propio.

Torres Amat numera como la Vulgata: la inscripción del salmo es un
versículo más, y el impreso le pone número («1. Salmo de David cuando
temeroso iba huyendo de su hijo Absalom»). Ese número se conserva; lo que
hay que añadir es que ese texto es título y no cuerpo.

Se marca dentro del propio versículo con

    <seg type="x-psalm-title">…</seg>

y no con <title type="psalm">. Probado con osis2mod e imp2vs sobre un módulo
Vulg: SWORD convierte <title> en <h3>, la disponibilidad de Biblia Elim
trata los encabezados como no-cuerpo, el versículo queda «Missing» y el
visor lo suple con Reina-Valera 1909 -- que trae título y primer versículo
juntos, duplicando el texto --. El <seg> sale como
<span class="x-psalm-title">: la clave sigue siendo Ps 3:1, el texto está
presente y la marca llega hasta el visor.

Qué versículos son título no se deduce del texto: lo dice el facsímil, y
cada caso lleva su hoja (ver parche_facsimil.TITULOS).
"""
import html

TIPO_TITULO = "x-psalm-title"


def marca_titulo(titulo, cuerpo=""):
    """Contenido OSIS de un versículo cuyo comienzo es título de salmo.

    cuerpo va detrás, sin marcar, cuando el impreso da en el mismo
    versículo el título y el primer renglón del salmo (Sal 52:1).
    """
    titulo = titulo.strip()
    if not titulo:
        raise ValueError("título vacío")
    out = f'<seg type="{TIPO_TITULO}">{html.escape(titulo, quote=False)}</seg>'
    cuerpo = cuerpo.strip()
    if cuerpo:
        out += " " + html.escape(cuerpo, quote=False)
    return out
