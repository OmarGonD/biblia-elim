#!/usr/bin/env python3
"""Pipeline de fichas por versículo (Fase 3).

  solicitud -> generador (intercambiable) -> validación (JSON Schema + semántica, 1 reintento)
            -> caché por (ref, hash del prompt, modelo) -> ensamblado en data/fichas_v3/

Generadores (misma interfaz, mismo esquema, misma caché y misma validación):
  manual : las respuestas las escribe una persona/agente y se cargan con `cargar`; no hay llamada externa.
  api    : Messages API de Anthropic, o Batch API con `batch-export` / `batch-import`.
Cambiar de generador no cambia nada más: la caché se separa por modelo.

Uso:
  pipeline.py solicitud John.1.1                 imprime la solicitud
  pipeline.py estado John.1                      cuántos versículos hay en caché
  pipeline.py cargar John.1.1 respuesta.json     valida y guarda una respuesta manual
  pipeline.py generar John.1 --generador api --modelo claude-sonnet-5-5
  pipeline.py batch-export John.1 lote.jsonl --modelo claude-sonnet-5-5
  pipeline.py batch-import resultados.jsonl --modelo claude-sonnet-5-5
  pipeline.py ensamblar John.1 --modelo manual-claude-sonnet-5-5
"""
import argparse, glob, hashlib, json, os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import alinear, contexto, tagnt

RAIZ = alinear.RAIZ
PROMPT = os.path.join(RAIZ, "prompts", "enriquecer_ficha.md")
ESQUEMA = os.path.join(RAIZ, "schemas", "ficha_enriquecida.schema.json")
CACHE = os.path.join(RAIZ, "data", "fichas_cache")
SALIDA = os.path.join(RAIZ, "data", "fichas_v3")
ERRORES = os.path.join(RAIZ, "logs", "fichas_errores.jsonl")
MODELO_MANUAL = "manual-claude-sonnet-5-5"
# Palabras de función muy frecuentes: solo campos básicos (artículo, καί, δέ, γάρ), salvo variante o ambigüedad.
BASICAS = {"G3588", "G2532", "G1161", "G1063"}
CAMPOS_BASICOS = {"pos_tisch", "strong", "glosa_interlineal", "rango_semantico", "construccion", "nivel_certeza"}
RE_VERSIONES = re.compile(r"\b(RVR\s?-?\s?\d*|Reina[- ]Valera\s*1960|NVI|LBLA|NTV|DHH|KJV|NIV|ESV|NASB|BJ)\b")
VERSIONES = [("SpaRV", "sparv"), ("TorresAmat", "torresamat")]   # ambas de dominio público


def prompt_texto():
    return open(PROMPT, encoding="utf-8").read()


def prompt_hash():
    return hashlib.sha256(prompt_texto().encode("utf-8")).hexdigest()[:12]


class Pendiente(Exception):
    """El generador no produce la respuesta por sí mismo (manual)."""


# ---------------------------------------------------------------- datos
class Datos:
    def __init__(self):
        self.tisch = alinear.leer_tisch()
        self.al = json.load(open(alinear.SALIDA, encoding="utf-8"))["v"]
        self.pals, self.por_clave = {}, {}
        for p in tagnt.leer():
            self.pals.setdefault(p.ref, []).append(p)
            self.por_clave[p.clave] = p

    def versiculos(self, prefijo):
        """Refs 'John.1.1' que empiezan por 'John', 'John.1' o 'John.1.1' (versificación Tisch)."""
        refs = ["%s.%d.%d" % k for k in sorted(self.tisch)]
        if prefijo.count(".") == 2:
            return [prefijo] if prefijo in refs else []
        return [r for r in refs if r.startswith(prefijo + ".")]

    def versiones(self, k):
        if getattr(self, "_versiones", None) is None:
            self._versiones = textos_versiones()
        return [{"version": e["version"], "texto": e["texto"]} for e in self._versiones.get(k, [])
                if "aviso" not in e]

    def texto(self, k):
        return " ".join(t["forma"] for t in self.tisch[k]) if k in self.tisch else None


def _orto(a, b):
    """Diferencia solo ortográfica (Ἡλείας/Ἠλίας, ῥαββεί/ῥαββί): igual tras ει→ι."""
    f = lambda x: alinear.norm(x).replace("ει", "ι")
    return f(a) == f(b)


def _calidad(flag):
    return flag.split("+")[0]


def _marcada(w):
    return bool(w.get("variante") or w.get("no_en_na28") or w.get("lectura_tagnt"))


def solicitud(datos, ref):
    o, c, v = ref.split(".")
    k = (o, int(c), int(v))
    al = {x[0]: x for x in datos.al[ref]}
    palabras = []
    for t in datos.tisch[k]:
        w = {"pos": t["pos"], "forma": t["forma"], "lema": t["lema"], "strong": t["strong"],
             "morfologia": t["morph"]}
        _, clave, _ = al[t["pos"]]
        flag = False
        if clave:
            p = datos.por_clave[clave]
            pals = datos.pals[p.ref]
            inf = contexto.info(pals, pals.index(p))
            w.update({"morfologia_tagnt": inf["morfologia_tagnt"], "glosa_tagnt": inf["glosa_tagnt"]})
            if inf["strong_tagnt"] != t["strong"]:
                w["strong_tagnt"] = inf["strong_tagnt"]
            distinta = (_calidad(al[t["pos"]][2]) == "strong"
                        and not _orto(t["forma"], p.griego))
            if distinta:
                # Tisch lee otra cosa que el TAGNT: `ediciones` son las que leen la forma del TAGNT, no la de Tisch
                w["lectura_tagnt"] = {"forma": p.griego.strip(".,;·¶ "), "ediciones": inf["ediciones"]}
            elif inf["variante"]:                     # solo se envían cuando hay algo que decir
                w.update({"variante": True, "ediciones": inf["ediciones"], "ausente_en": inf["ausente_en"]})
            if not p.en("NA28") and not distinta:
                w["no_en_na28"] = True
            for extra in ("caso_regido", "caso_regido_ambiguo"):
                if extra in inf:
                    w[extra] = inf[extra]
            flag = bool(inf["variante"] or w.get("no_en_na28") or distinta or "caso_regido_ambiguo" in inf)
        w["nivel"] = "basico" if t["strong"] in BASICAS and not flag else "completo"
        palabras.append(w)
    sol = {"ref": ref, "texto_griego": datos.texto(k),
           "anterior": datos.texto((o, int(c), int(v) - 1)), "siguiente": datos.texto((o, int(c), int(v) + 1)),
           "palabras": palabras}
    if any(_marcada(w) for w in palabras):       # solo en versículos con variantes: lo único que se puede citar
        sol["textos_pd"] = datos.versiones(k)
    return sol


# ---------------------------------------------------------------- validación
def validar(cards, sol):
    """Lista de errores (vacía si es válida). JSON Schema + reglas semánticas."""
    import jsonschema
    esquema = json.load(open(ESQUEMA, encoding="utf-8"))
    errores = ["%s: %s" % ("/".join(map(str, e.absolute_path)) or "raíz", e.message)
               for e in jsonschema.Draft202012Validator(esquema).iter_errors(cards)]
    if errores:
        return errores[:10]
    esperadas = [(w["pos"], w["strong"]) for w in sol["palabras"]]
    if [(c["pos_tisch"], c["strong"]) for c in cards] != esperadas:
        return ["las fichas no coinciden con las palabras pedidas (pos_tisch/strong/orden)"]
    for c, w in zip(cards, sol["palabras"]):
        ident = "pos %d %s" % (w["pos"], w["forma"])
        if w["nivel"] == "basico":
            extra = [k for k, v in c.items() if k not in CAMPOS_BASICOS and v is not None]
            if extra or c["nivel_certeza"] != "alto":
                errores.append("%s: palabra básica solo admite campos básicos (%s)" % (ident, extra))
        marcada = _marcada(w)
        if marcada and not c["variantes_textuales"]:
            errores.append("%s: falta variantes_textuales (variante, lectura distinta o ausente en NA28)" % ident)
        if not marcada and c["variantes_textuales"]:
            errores.append("%s: variantes_textuales sin variante en el TAGNT" % ident)
        if "caso_regido_ambiguo" in w and c["nivel_certeza"] == "alto":
            errores.append("%s: caso regido ambiguo, nivel_certeza no puede ser alto" % ident)
        if w.get("caso_regido") and c["construccion"] and w["caso_regido"][:5] not in c["construccion"].lower():
            errores.append("%s: construccion no menciona el caso regido (%s)" % (ident, w["caso_regido"]))
        texto = json.dumps(c, ensure_ascii=False)
        if RE_VERSIONES.search(texto) or "traducciones_comparadas" in c:
            errores.append("%s: menciona una versión o cita traducciones" % ident)
    return errores[:10]


# ---------------------------------------------------------------- caché y registro
class Cache:
    def __init__(self, modelo, raiz=CACHE):
        self.modelo, self.hash = modelo, prompt_hash()
        self.dir = os.path.join(raiz, re.sub(r"[^\w.-]", "_", modelo), self.hash)

    def ruta(self, ref):
        return os.path.join(self.dir, ref + ".json")

    def get(self, ref):
        r = self.ruta(ref)
        return json.load(open(r, encoding="utf-8"))["fichas"] if os.path.exists(r) else None

    def put(self, ref, cards):
        os.makedirs(self.dir, exist_ok=True)
        with open(self.ruta(ref), "w", encoding="utf-8") as f:
            json.dump({"ref": ref, "modelo": self.modelo, "prompt_hash": self.hash,
                       "generado": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "fichas": cards},
                      f, ensure_ascii=False, indent=1)
            f.write("\n")


def registrar_error(ref, modelo, intento, errores, ruta=ERRORES):
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    with open(ruta, "a", encoding="utf-8") as f:
        f.write(json.dumps({"t": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "ref": ref, "modelo": modelo,
                            "intento": intento, "errores": errores}, ensure_ascii=False) + "\n")


# ---------------------------------------------------------------- generadores
class GeneradorManual:
    modelo = MODELO_MANUAL

    def generar(self, sol, error_previo=None):
        raise Pendiente(sol["ref"])


def _extraer_json(texto):
    texto = texto.strip()
    m = re.match(r"^```(?:json)?\s*(.*?)\s*```$", texto, re.S)
    return json.loads(m.group(1) if m else texto)


class GeneradorAPI:
    """Messages API (síncrona) y Batch API. Requiere ANTHROPIC_API_KEY y `pip install anthropic`."""

    def __init__(self, modelo, cliente=None, max_tokens=16000):
        self.modelo, self.max_tokens, self._cliente = modelo, max_tokens, cliente

    @property
    def cliente(self):
        if self._cliente is None:
            import anthropic
            self._cliente = anthropic.Anthropic()
        return self._cliente

    def params(self, sol, error_previo=None):
        contenido = json.dumps(sol, ensure_ascii=False)
        if error_previo:
            contenido += "\n\nTu respuesta anterior no fue válida: %s\nCorrígela y devuelve solo el arreglo JSON." % (
                error_previo if isinstance(error_previo, str) else "; ".join(error_previo))
        return {"model": self.modelo, "max_tokens": self.max_tokens,
                "system": [{"type": "text", "text": prompt_texto(), "cache_control": {"type": "ephemeral"}}],
                "messages": [{"role": "user", "content": contenido}]}

    def generar(self, sol, error_previo=None):
        r = self.cliente.messages.create(**self.params(sol, error_previo))
        return _extraer_json("".join(b.text for b in r.content if getattr(b, "type", "") == "text"))


def batch_export(datos, refs, ruta, modelo, max_tokens=16000):
    g = GeneradorAPI(modelo, max_tokens=max_tokens)
    with open(ruta, "w", encoding="utf-8") as f:
        for ref in refs:
            f.write(json.dumps({"custom_id": ref, "params": g.params(solicitud(datos, ref))},
                               ensure_ascii=False) + "\n")


def batch_import(datos, ruta, modelo, cache=None, log=ERRORES):
    """Lee los resultados de la Batch API (JSONL) y los pasa por la misma validación y caché."""
    cache = cache or Cache(modelo)
    res = {"ok": 0, "error": 0}
    for linea in open(ruta, encoding="utf-8"):
        r = json.loads(linea)
        ref = r["custom_id"]
        try:
            msg = r["result"]["message"]
            cards = _extraer_json("".join(b["text"] for b in msg["content"] if b.get("type") == "text"))
            err = validar(cards, solicitud(datos, ref))
        except Exception as e:  # resultado no exitoso o JSON roto
            err = ["resultado no utilizable: %s" % e]
        if err:
            registrar_error(ref, modelo, "batch", err, log)
            res["error"] += 1
        else:
            cache.put(ref, cards)
            res["ok"] += 1
    return res


# ---------------------------------------------------------------- proceso
def procesar(datos, ref, gen, cache, log=ERRORES):
    """-> 'cache' | 'ok' | 'pendiente' | 'fallo'. Un reintento con el error por delante."""
    if cache.get(ref) is not None:
        return "cache"
    sol, error = solicitud(datos, ref), None
    for intento in (1, 2):
        try:
            cards = gen.generar(sol, error)
        except Pendiente:
            return "pendiente"
        except Exception as e:
            error = ["fallo del generador: %s" % e]
        else:
            error = validar(cards, sol)
            if not error:
                cache.put(ref, cards)
                return "ok"
        registrar_error(ref, cache.modelo, intento, error, log)
    return "fallo"


def cargar(datos, ref, ruta, modelo=MODELO_MANUAL, log=ERRORES):
    """Valida y guarda una respuesta escrita a mano (generador manual)."""
    cards = json.load(open(ruta, encoding="utf-8"))
    err = validar(cards, solicitud(datos, ref))
    if err:
        registrar_error(ref, modelo, "manual", err, log)
        return err
    Cache(modelo).put(ref, cards)
    return []


# ---------------------------------------------------------------- ensamblado
RE_OCR = re.compile(r"(?:(?<=\s)|^)([$%£/=+\]\[|]|[bcdfghjklmnpqrstvwxzBCDFGHJKLMNPQRSTVWXZ])(?=\s|[,.;:]|$)")
AVISO_OCR = "El módulo SWORD trae caracteres sueltos que parecen errores de OCR en este versículo; el texto se muestra sin corregir."


def _texto_plano(t):
    """Equivalente a stripText de SWORD (sin notas ni marcado): quita etiquetas y normaliza espacios.
    No corrige el contenido: los errores de OCR de TorresAmat son texto literal del módulo."""
    return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", t)).strip()


def textos_versiones():
    """{(osis, cap, ver): [{version, texto}]} con SpaRV y TorresAmat y el nombre EXACTO del módulo."""
    import subprocess
    res = {}
    for modulo, conf in VERSIONES:
        ruta = os.path.expanduser("~/.sword/mods.d/%s.conf" % conf)
        desc = re.search(r"^Description=(.*)$", open(ruta, encoding="utf-8").read(), re.M)[1].strip()
        out = subprocess.run(["mod2imp", modulo], capture_output=True, check=True).stdout.decode("utf-8", "replace")
        actual = None
        for linea in out.splitlines():
            m = re.match(r"\$\$\$(.+) (\d+):(\d+)$", linea)
            if m:
                o = alinear.SWORD_A_OSIS.get(m[1])
                actual = (o, int(m[2]), int(m[3])) if o else None
            elif actual and linea.strip():
                res.setdefault(actual, []).append({"version": desc, "texto": _texto_plano(linea)})
    for k, lista in res.items():
        por = {}
        for x in lista:
            por.setdefault(x["version"], []).append(x["texto"])
        res[k] = []
        for v, t in por.items():
            e = {"version": v, "texto": " ".join(t)}
            if RE_OCR.search(e["texto"]):
                e["aviso"] = AVISO_OCR
            res[k].append(e)
    return res


def ensamblar(datos, refs, modelo, versiones=None, cache=None, salida_dir=None):
    cache = cache or Cache(modelo)
    versiones = versiones if versiones is not None else textos_versiones()
    salida = {}
    for ref in refs:
        cards = cache.get(ref)
        if cards is None:
            continue
        sol = solicitud(datos, ref)
        o, c, v = ref.split(".")
        al = {x[0]: x for x in datos.al[ref]}
        for card, w in zip(cards, sol["palabras"]):
            f = {"ref": ref, "pos_tisch": w["pos"], "strong": w["strong"], "forma": w["forma"], "lema": w["lema"]}
            f.update({k: card[k] for k in ("glosa_interlineal", "rango_semantico", "construccion",
                                          "sentido_en_contexto", "matiz", "variantes_textuales",
                                          "notas_traduccion", "otros_usos", "nivel_certeza")})
            _, clave, calidad = al[w["pos"]]
            f["alineacion"] = calidad or "sin_pareja"
            if clave:
                p = datos.por_clave[clave]
                f.update(contexto.info(datos.pals[p.ref], datos.pals[p.ref].index(p)))
                f.pop("glosa_tagnt", None)          # inglés, solo orientativa: no se distribuye en la ficha
                if not p.en("NA28"):
                    f["no_en_na28"] = True
                if w.get("lectura_tagnt"):
                    f["lectura_tagnt"] = w["lectura_tagnt"]
                    f.pop("variante", None); f.pop("ausente_en", None); f.pop("no_en_na28", None)
            else:
                f["tagnt"], f["revisar_tagnt"] = None, True
            f["traducciones_comparadas"] = versiones.get((o, int(c), int(v)))
            f["modelo"] = modelo
            salida.setdefault("%s.%02d" % (o, int(c)), []).append(f)
    destino = salida_dir or SALIDA
    os.makedirs(destino, exist_ok=True)
    for nombre, lista in salida.items():
        with open(os.path.join(destino, nombre + ".json"), "w", encoding="utf-8") as fh:
            json.dump(lista, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
    return {n: len(l) for n, l in salida.items()}


# ---------------------------------------------------------------- CLI
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("orden", choices=["solicitud", "estado", "cargar", "generar", "batch-export", "batch-import",
                                      "ensamblar"])
    ap.add_argument("args", nargs="*")
    ap.add_argument("--generador", choices=["manual", "api"], default="manual")
    ap.add_argument("--modelo", default=None)
    a = ap.parse_args(argv)
    datos = Datos()
    modelo = a.modelo or (MODELO_MANUAL if a.generador == "manual" else "claude-sonnet-5-5")
    if a.orden == "solicitud":
        print(json.dumps(solicitud(datos, a.args[0]), ensure_ascii=False, indent=1))
    elif a.orden == "estado":
        refs, cache = datos.versiculos(a.args[0]), Cache(modelo)
        hechos = [r for r in refs if cache.get(r) is not None]
        print("%s: %d/%d versículos en caché (%s, prompt %s)" % (a.args[0], len(hechos), len(refs), modelo, cache.hash))
    elif a.orden == "cargar":
        err = cargar(datos, a.args[0], a.args[1], modelo)
        print("OK" if not err else "\n".join(err))
        return 1 if err else 0
    elif a.orden == "generar":
        gen = GeneradorManual() if a.generador == "manual" else GeneradorAPI(modelo)
        cache, cuenta = Cache(gen.modelo), {}
        for ref in datos.versiculos(a.args[0]):
            e = procesar(datos, ref, gen, cache)
            cuenta[e] = cuenta.get(e, 0) + 1
        print(cuenta)
    elif a.orden == "batch-export":
        refs = [r for r in datos.versiculos(a.args[0]) if Cache(modelo).get(r) is None]
        batch_export(datos, refs, a.args[1], modelo)
        print("%d solicitudes en %s" % (len(refs), a.args[1]))
    elif a.orden == "batch-import":
        print(batch_import(datos, a.args[0], modelo))
    elif a.orden == "ensamblar":
        print(ensamblar(datos, datos.versiculos(a.args[0]), modelo))
    return 0


if __name__ == "__main__":
    sys.exit(main())
