"""Separa el front-matter de un libro del texto canónico.

La introducción de un libro (prólogo del Salterio, del Job, título de
Proverbios) llega a `corriente` como `sigue` y como versos espurios:
el OCR lee un 1, un 7, un 8 en la prosa y `segmenta` abre capítulos.
Esos candidatos se alinean contra el canon y desplazan el texto real.

Esta fase corre *antes* de `segmenta` / `alinea_capitulos`.  Tras un
cambio de libro retiene la corriente como front-matter hasta que hay
evidencia de capítulo canónico: una racha de versos a principio de
renglón 1, 2, 3…  Un dígito 1 suelto no basta — ese fue el bug.

Si el marcador `libro` cae a mitad de un capítulo (la cabecera corrida
se retrasa), la ventana sigue a la numeración previa y no se recorta.
"""


def _es_vers(s):
    return s[0] == "vers"


def _es_inicio(s):
    return _es_vers(s) and len(s) > 4 and s[4]


def _nums_verso(sucesos):
    return [s[1] for s in sucesos if _es_vers(s)]


def _textos(sucesos):
    out = []
    for s in sucesos:
        if _es_vers(s):
            out.append(s[2])
        elif s[0] == "sigue":
            out.append(s[1])
    return out


def _libro_de(s):
    if s[0] != "libro":
        return None
    if len(s) > 1 and s[1]:
        return s[1]
    return None


def _cands_de(s):
    if _es_vers(s) and len(s) > 3 and s[3]:
        c = s[3]
        if isinstance(c, (set, list, tuple)) and len(c) == 1:
            return next(iter(c))
    if s[0] == "sigue" and len(s) > 2 and s[2]:
        c = s[2]
        if isinstance(c, (set, list, tuple)) and len(c) == 1:
            return next(iter(c))
    return None


def run_es_apertura(run):
    """¿Esta racha de números a principio de renglón es un capítulo 1 real?

    1, 2, 3… sí.  1, 7, 8, 18, 23… no.  Un 1 suelto no.
    """
    if not run or run[0] != 1 or 2 not in run:
        return False
    idx2 = run.index(2)
    if idx2 > 2:
        return False
    if 3 in run:
        return True
    consec = sum(1 for a, b in zip(run, run[1:]) if 0 < b - a <= 2)
    return consec >= 2 and max(run) <= max(len(run) + 8, 12)


def indice_apertura(sucesos):
    """Índice del primer suceso del texto canónico, o None."""
    starts = [(i, s[1]) for i, s in enumerate(sucesos) if _es_inicio(s)]
    for k, (i, n) in enumerate(starts):
        if n != 1:
            continue
        run = []
        for _j, m in starts[k:]:
            if m == 1 and run:
                break
            run.append(m)
        if run_es_apertura(run):
            return i
    for i, s in enumerate(sucesos):
        if s[0] == "cap" and s[1] == 1:
            return i
    return None


def es_continuacion(window, prev_nums):
    """La ventana sigue el capítulo que ya estaba en curso, no un prólogo.

    Un solo número que encaja (el 18 de la intro de Salmos tras Job 42:17)
    no basta: el OCR siembra cifras sueltas en la prosa.
    """
    nums = _nums_verso(window)[:12]
    if not nums:
        return False
    if prev_nums:
        last = prev_nums[-1]
        continua = [n for n in nums if n == last or n == last + 1 or
                    (last >= 8 and 0 <= n - last <= 3)]
        if len(continua) >= 2:
            return True
    if 1 not in nums[:8] and min(nums) >= 8:
        starts = [s[1] for s in window if _es_inicio(s)][:8]
        if len(starts) >= 3:
            return True
    return False


def parece_front_matter(front):
    """El prefijo es prólogo, no un trozo de capítulo ya empezado.

    Si recortáramos hasta el primer 1-2-3 de un libro cuyo cap. 1
    el OCR numeró mal, nos comeríamos medio 1 Corintios.  Una racha
    densa de versos consecutivos no es introducción.
    """
    if not front:
        return False
    n_sigue = sum(1 for s in front if s[0] == "sigue")
    n_vers = sum(1 for s in front if _es_vers(s))
    starts = [s[1] for s in front if _es_inicio(s)]
    if len(starts) >= 5:
        consec = sum(1 for a, b in zip(starts, starts[1:]) if 0 < b - a <= 2)
        if consec >= 3:
            return False
    if len(front) <= 8 and n_vers <= 1:
        return True
    if n_sigue >= max(4, n_vers):
        return True
    if starts and 2 not in starts and 3 not in starts:
        if max(starts) > 10 or len(starts) <= 4:
            return True
    if starts and max(starts) > 20 and 2 not in starts[:8]:
        return True
    return False


def _adivina_libro(sucesos, desde):
    for s in sucesos[desde + 1:desde + 40]:
        osis = _cands_de(s)
        if osis:
            return osis
    return None


def separa_front_matter(sucesos):
    """(corriente canónica, {osis: [fragmentos de intro]}).

    Los fragmentos de intro no se entregan a `segmenta`.
    """
    out = []
    intros = {}
    prev_nums = []
    i = 0
    n = len(sucesos)
    while i < n:
        s = sucesos[i]
        if s[0] != "libro":
            out.append(s)
            if _es_vers(s):
                prev_nums.append(s[1])
                if len(prev_nums) > 24:
                    prev_nums = prev_nums[-24:]
            i += 1
            continue

        book = _libro_de(s) or _adivina_libro(sucesos, i)
        j = i + 1
        while j < n and sucesos[j][0] != "libro":
            j += 1
        window = sucesos[i + 1:j]

        if es_continuacion(window, prev_nums):
            out.append(s)
            i += 1
            continue

        ap = indice_apertura(window)
        if ap is None or ap == 0:
            out.append(s)
            i += 1
            continue

        front = window[:ap]
        if not parece_front_matter(front) or es_continuacion(front, prev_nums):
            out.append(s)
            i += 1
            continue

        frags = [t for t in _textos(front) if t and t.strip()]
        if frags and book:
            intros.setdefault(book, []).extend(frags)
        out.append(s)
        i = i + 1 + ap
        prev_nums = []
    return out, intros
