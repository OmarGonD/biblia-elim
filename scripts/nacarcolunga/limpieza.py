"""Errores de OCR que se corrigen con una regla general o con una lectura
documentada del facsímil.

No moderniza ni normaliza: cada regla repone lo que está impreso y se ha
cotejado con las hojas de Princeton (ver TASKS.md, NACAR-OCR-102).
"""
import json
import os
import re
from collections import Counter

DIR = os.path.dirname(os.path.abspath(__file__))
ERRATAS = os.path.join(DIR, "erratas.json")

LETRAS = "A-Za-zÁÉÍÓÚÜÑáéíóúüñ"
RE_PALABRA = re.compile(f"[{LETRAS}]+")


def vocabulario(textos):
    """Frecuencia de cada palabra (con su caja) en el propio texto."""
    return Counter(w for t in textos for w in RE_PALABRA.findall(t or ""))


# --- «!» leído como «l» -------------------------------------------------------
# El cierre de exclamación de esta tipografía lleva un remate que Tesseract
# lee como «l»: «¡oh Yavel,», «¡oh Diosl,», «caso!» → «casol» (Job 30:20),
# «vivo!» → «vivol» (Heb 9:14). Se repone solo si:
#   - hay un «¡» sin cerrar delante, o un «oh» justo antes;
#   - detrás viene fin, puntuación o palabra/signo que abre frase;
#   - la raíz es palabra corriente (>= 20) y la forma con «l» es rara
#     frente a ella (< 1/20): «el», «mil», «Israel», «aquel» no se tocan.
RE_CIERRE_L = re.compile(
    rf"(?<![{LETRAS}])([{LETRAS}]+)l"
    rf"(?=$|[,;:.)»”]|\s+[A-ZÁÉÍÓÚÑ¡¿«(])")
RE_OH = re.compile(r"\b[jJ]?[oO]h\s*$")
MIN_RAIZ = 20
PROPORCION = 20


def cierra_exclamaciones(texto, vocab):
    out, prev = [], 0
    for m in RE_CIERRE_L.finditer(texto):
        raiz = m.group(1)
        antes = "".join(out) + texto[prev:m.start()]
        abierta = antes.count("¡") > antes.count("!") or RE_OH.search(antes)
        if (abierta and vocab[raiz] >= MIN_RAIZ
                and vocab[raiz + "l"] * PROPORCION <= vocab[raiz]):
            out.append(texto[prev:m.start()] + raiz + "!")
            prev = m.end()
    out.append(texto[prev:])
    return "".join(out)


# --- Guion de fin de renglón leído como «.» o «:» -----------------------------
# «multi. | plicado» (Sal 3:2 impreso), «Ab: | salón» (Sal 3:1 impreso),
# «san: | gre» (Éx 24:6). Se une solo si la palabra entera aparece al menos
# tantas veces como el trozo izquierdo suelto (y dos como mínimo), y si el
# trozo derecho empieza en minúscula y tiene dos letras o más: una vocal
# sola no abre renglón en la partición castellana.
RE_FIN_LEIDO = re.compile(f"([{LETRAS}]+)[.:]$")
RE_INICIO = re.compile("([a-záéíóúüñ]{2,})")


def repara_guiones(trozos, vocab_min):
    out = [t.strip() for t in trozos if t and t.strip()]
    for i in range(len(out) - 1):
        m = RE_FIN_LEIDO.search(out[i])
        n = RE_INICIO.match(out[i + 1])
        if not (m and n):
            continue
        izq = m.group(1).lower()
        entera = izq + n.group(1).lower()
        if vocab_min[entera] >= 2 and vocab_min[entera] >= vocab_min[izq]:
            out[i] = out[i][:-1] + "-"
    return out


def vocabulario_min(textos):
    return Counter(w.lower() for t in textos for w in RE_PALABRA.findall(t or ""))


# --- Apóstrofo y guion bajo que el OCR pone donde el impreso no -----------
# Cotejado en Princeton: «reci- | bir» (Gn 4:11) sale «reci'bir»; «son las»
# (Gn 6:9) sale «son'las»; «bue- | nas» (Mt 7:11) sale «bue'nas»;
# «Jerusalén» (Is 4:3) sale «Jeru'salén»; «dijo» (Jc 11:35) NO se toca,
# porque quitar el apóstrofo dejaría «djo», que no es la palabra.
# El guion bajo es el guion de fin de renglón: «tri_bus» es «tribus»
# (Jos 23). No se une si el resultado no es ya palabra de este texto.
RE_APOSTROFO = re.compile(rf"([{LETRAS}]+)'([{LETRAS}]+)")
RE_GUION_BAJO = re.compile(rf"([{LETRAS}]+)_+([{LETRAS}]+)")
FUNCION = {
    "a", "e", "o", "u", "y", "al", "del", "el", "la", "las", "los", "lo",
    "le", "les", "me", "te", "se", "os", "nos", "su", "sus", "mi", "tu",
    "un", "una", "en", "de", "que", "no", "si", "es", "son", "ha", "he",
    "por", "con", "sin",
}
MIN_PALABRA = 2


def repara_cortes_ocr(texto, vocab_min):
    """Quita el apóstrofo o el guion bajo espurios, sin modernizar.

    Solo si el resultado es una palabra que este mismo texto ya usa, o si
    el apóstrofo separa dos palabras que también usa. Lo demás se deja.
    """
    def guion(m):
        junto = (m.group(1) + m.group(2)).lower()
        if vocab_min[junto] >= MIN_PALABRA:
            return m.group(1) + m.group(2)
        return m.group(0)

    def apostrofo(m):
        izq, der = m.group(1), m.group(2)
        if der[:1].isupper():
            return m.group(0)
        il, dl = izq.lower(), der.lower()
        junto = il + dl
        izq_es = il in FUNCION or vocab_min[il] >= MIN_PALABRA
        der_es = dl in FUNCION or vocab_min[dl] >= MIN_PALABRA
        # Primero la palabra entera, aunque el corte parezca dos
        # trozos («bue'nas», «Jeru'salén», «reve'lado»). «a'la» no
        # llega aquí: «ala» tiene menos de seis letras.
        if len(junto) >= 6 and vocab_min[junto] >= MIN_PALABRA:
            return izq + der
        # «son'las», «tómala'y», «ver'esto»: un espacio comido.
        if izq_es and der_es and max(len(il), len(dl)) >= 3:
            return izq + " " + der
        return m.group(0)

    return RE_APOSTROFO.sub(apostrofo, RE_GUION_BAJO.sub(guion, texto))


# --- Lecturas documentadas ----------------------------------------------------

def carga_erratas(ruta=ERRATAS):
    with open(ruta, encoding="utf-8") as f:
        datos = json.load(f)
    for e in datos:
        campos = ("ref", "dice", "fuente") if e.get("alta") else (
            "ref", "lee", "dice", "fuente")
        for campo in campos:
            if not e.get(campo):
                raise ValueError(f"errata sin {campo}: {e}")
    return datos


def aplica_erratas(textos, erratas, avisos):
    """Sustituye la lectura del OCR por la impresa, solo si aparece una vez.

    Si el OCR cambia y la lectura ya no está -- o ya dice lo impreso: «iga
    Israel» sigue dentro de «Diga Israel» --, no se toca nada y queda un
    aviso. «alta» es la excepción: el verso no está en el módulo y el
    facsímil lo trae; se escribe ese texto y nada más.
    """
    for e in erratas:
        t = textos.get(e["ref"])
        if not t:
            if e.get("alta"):
                textos[e["ref"]] = e["dice"]
                continue
            avisos.append(f"{e['ref']}: errata no aplicada «{e.get('lee', '')}»")
            continue
        if e.get("alta"):
            continue
        # Ya aplicada: la lectura vieja desapareció, o era un trozo de la
        # nueva («iga» dentro de «Diga») y el verso ya dice lo impreso.
        if e["lee"] not in t or (e["lee"] in e["dice"] and e["dice"] in t):
            if e["dice"] in t:
                continue
            avisos.append(f"{e['ref']}: errata no aplicada «{e['lee']}»")
            continue
        if t.count(e["lee"]) != 1:
            avisos.append(f"{e['ref']}: errata no aplicada «{e['lee']}»")
            continue
        textos[e["ref"]] = t.replace(e["lee"], e["dice"])
