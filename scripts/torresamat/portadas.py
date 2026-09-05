"""
Detecta las páginas de título que abren cada libro.

Es el ancla que faltaba: en la primera página de un libro no hay cabecera
corrida -- lleva el título en grande en su lugar --, que es justo donde el
alineador más se equivocaba. Así se le colaron los cuatro primeros
versículos de San Juan al final de San Lucas.

Las láminas también traen letra grande, pero es basura de OCR; lo que
distingue a una portada es que esa letra grande forme palabras del
repertorio de títulos de la edición.
"""
import re, statistics
from load import X1, X2, TOP, BOT, CONF, TXT
from segment import agrupar_lineas, med_h, texto
from cabeceras import sin_tildes

PALABRAS = ("EVANGELIO", "APOSTOL", "EPISTOLA", "SANTO", "LIBRO", "LIBROS",
            "PROFECIA", "PROFETA", "HECHOS", "APOSTOLES", "CATHOLICA",
            "APOCALYPSI", "SALMOS", "PROVERBIOS", "SABIDURIA", "GENESIS",
            "EXODO", "LEVITICO", "NUMEROS", "DEUTERONOMIO", "JOSUE",
            "JUECES", "RUTH", "REYES", "PARALIPOMENON", "ESDRAS", "TOBIAS",
            "JUDITH", "ESTHER", "MACHABEOS", "ECLESIASTES", "ECLESIASTICO",
            "CANTAR", "LAMENTACIONES", "THRENOS", "BARUCH", "MALACHIAS",
            "PRIMERA", "SEGUNDA", "TERCERA", "PABLO", "SANTIAGO", "JUDAS")

def es_portada(ws):
    """¿Esta media hoja abre libro?"""
    if not ws:
        return False
    med = statistics.median(w[BOT] - w[TOP] for w in ws)
    if med <= 0:
        return False
    for l in agrupar_lineas(ws):
        if med_h(l) < med * 2.0:
            continue
        t = sin_tildes(texto(l)).upper()
        t = re.sub(r"[^A-Z ]", " ", t)
        for p in t.split():
            if len(p) >= 5 and p in PALABRAS:
                return True
    return False
