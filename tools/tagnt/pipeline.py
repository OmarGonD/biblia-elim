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
  export ANTHROPIC_API_KEY=...          (solo variable de entorno; nunca se escribe en un archivo)
  pipeline.py batch-enviar John.1        crea el lote (Batch API, modelo claude-sonnet-5-5, prompt cacheado)
  pipeline.py batch-estado <id>          estado del lote
  pipeline.py batch-traer <id> res.jsonl descarga, valida, guarda en caché y registra tokens reales en logs/uso_api.jsonl
  pipeline.py batch-enviar John.1 --reintento   un reintento con el error del primer intento
  pipeline.py batch-export John.1 lote.jsonl    (solo exporta las solicitudes, sin llamar a la API)
  pipeline.py batch-import resultados.jsonl
  pipeline.py ensamblar John.1 --modelo manual-claude-sonnet-5-5
"""
import argparse, collections, glob, hashlib, json, os, re, subprocess, sys, time
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
# Rango del módulo Tisch que no se genera: el módulo oficial lo entrega roto (data/sources/NOTAS_TISCH.md)
EXCLUIDOS = [("John", 8, 12, 53)]
UMBRAL_STD = 0.25     # una palabra aporta una ref estándar al versículo si ≥25 % de sus palabras caen en ella


def excluido(ref):
    o, c, v = ref.split(".")
    return any(o == eo and int(c) == ec and ev0 <= int(v) <= ev1 for eo, ec, ev0, ev1 in EXCLUIDOS)


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
        """Refs 'John.1.1' que empiezan por 'John', 'John.1' o 'John.1.1' (versificación Tisch), sin los excluidos."""
        refs = ["%s.%d.%d" % k for k in sorted(self.tisch)]
        if prefijo.count(".") == 2:
            refs = [prefijo] if prefijo in refs else []
        else:
            refs = [r for r in refs if r.startswith(prefijo + ".")]
        return [r for r in refs if not excluido(r)]

    # ---- numeración estándar (KJV, la de SpaRV): el módulo Tisch numera distinto en algunos pasajes
    def ref_estandar_clave(self, clave):
        return self.por_clave[clave].ref_estandar if clave else None

    def std_refs(self, ref):
        """Lista ordenada de refs estándar a las que corresponde un versículo del módulo Tisch."""
        cnt = collections.Counter()
        for _, clave, _ in self.al[ref]:
            if clave:
                cnt[self.ref_estandar_clave(clave)] += 1
        tot = sum(cnt.values())
        refs = sorted((r for r, n in cnt.items() if tot and n / tot >= UMBRAL_STD),
                      key=lambda r: tuple(int(x) if x.isdigit() else x for x in r.split(".")))
        return refs or [ref]

    def versiones(self, ref, textos=None):
        """Textos de SpaRV y TorresAmat en la numeración estándar para un versículo del módulo Tisch:
        [{version, ref, texto}]. TorresAmat se omite si el versículo trae marcas de OCR."""
        if textos is None:
            if getattr(self, "_versiones", None) is None:
                self._versiones = textos_versiones()
            textos = self._versiones
        out = []
        for r in self.std_refs(ref):
            o, c, v = r.split(".")
            for e in textos.get((o, int(c), int(v)), []):
                if "aviso" in e:
                    continue
                out.append({"version": e["version"], "ref": r, "texto": e["texto"]})
        return out

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
    if excluido(ref):
        raise ValueError("%s está excluido (defecto del módulo Tisch, ver NOTAS_TISCH.md)" % ref)
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
                if len(inf["ediciones"]) == len(tagnt.EDICIONES):
                    w["lectura_tagnt"]["propia_de_tisch"] = True   # ninguna edición del TAGNT lee lo que lee Tisch
            elif inf["variante"]:                     # solo se envían cuando hay algo que decir
                w.update({"variante": True, "ediciones": inf["ediciones"], "ausente_en": inf["ausente_en"]})
            if not p.en("NA28") and not distinta:
                w["no_en_na28"] = True
            for extra in ("caso_regido", "caso_regido_ambiguo"):
                if extra in inf:
                    w[extra] = inf[extra]
            flag = bool(inf["variante"] or w.get("no_en_na28") or distinta or "caso_regido_ambiguo" in inf)
        # el módulo Tisch etiqueta a veces mal (ὅ relativo como G3588): si el TAGNT discrepa, no es palabra básica
        w["nivel"] = "basico" if t["strong"] in BASICAS and not flag and "strong_tagnt" not in w else "completo"
        palabras.append(w)
    sol = {"ref": ref, "texto_griego": datos.texto(k),
           "anterior": datos.texto((o, int(c), int(v) - 1)), "siguiente": datos.texto((o, int(c), int(v) + 1)),
           "palabras": palabras}
    if any(_marcada(w) for w in palabras):       # solo en versículos con variantes: lo único que se puede citar
        sol["textos_pd"] = datos.versiones(ref)
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
        lt = w.get("lectura_tagnt")
        if lt and c["variantes_textuales"] and ("Tisch" not in c["variantes_textuales"]
                                                 or alinear.norm(lt["forma"]) not in alinear.norm(c["variantes_textuales"])):
            errores.append("%s: variantes_textuales debe decir que es lectura de Tischendorf y qué leen las demás ediciones (%s)"
                           % (ident, lt["forma"]))
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

    def meta(self, ref):
        """Procedencia guardada junto a la respuesta: {modelo, prompt_hash, generado}."""
        with open(self.ruta(ref), encoding="utf-8") as f:
            d = json.load(f)
        return {k: d.get(k) for k in ("modelo", "prompt_hash", "generado")}

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


USO = os.path.join(RAIZ, "logs", "uso_api.jsonl")
PENDIENTES = os.path.join(RAIZ, "logs", "batch_pendientes.json")
MODELO_API = "claude-sonnet-5-5"


def cliente_anthropic():
    """Cliente de la API. La clave se lee SOLO de la variable de entorno ANTHROPIC_API_KEY (nunca de un archivo)."""
    if not os.environ.get("ANTHROPIC_API_KEY"):
        raise RuntimeError("Falta la variable de entorno ANTHROPIC_API_KEY (no se guarda en ningún archivo): "
                           "export ANTHROPIC_API_KEY=... antes de ejecutar.")
    import anthropic
    return anthropic.Anthropic()


def custom_id(ref):
    """La Batch API exige ^[a-zA-Z0-9_-]{1,64}$: John.1.1 -> John_1_1."""
    return ref.replace(".", "_")


def ref_de_custom_id(cid):
    return cid.replace("_", ".")


class GeneradorAPI:
    """Messages API (síncrona) y Batch API con caché del prompt de sistema. Clave: ANTHROPIC_API_KEY."""

    def __init__(self, modelo=MODELO_API, cliente=None, max_tokens=16000):
        self.modelo, self.max_tokens, self._cliente = modelo, max_tokens, cliente

    @property
    def cliente(self):
        if self._cliente is None:
            self._cliente = cliente_anthropic()
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
        registrar_uso(sol["ref"], self.modelo, getattr(r, "usage", None), "sync")
        return _extraer_json("".join(b.text for b in r.content if getattr(b, "type", "") == "text"))


def registrar_uso(ref, modelo, usage, via, ruta=None):
    """Tokens reales de una llamada (para medir el costo): entrada, salida y caché de prompt."""
    if usage is None:
        return
    get = (lambda k: usage.get(k)) if isinstance(usage, dict) else (lambda k: getattr(usage, k, None))
    ruta = ruta or USO
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    with open(ruta, "a", encoding="utf-8") as f:
        f.write(json.dumps({"t": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "ref": ref, "modelo": modelo, "via": via,
                            "input_tokens": get("input_tokens") or 0, "output_tokens": get("output_tokens") or 0,
                            "cache_creation_input_tokens": get("cache_creation_input_tokens") or 0,
                            "cache_read_input_tokens": get("cache_read_input_tokens") or 0}) + "\n")


def _errores_previos(ruta=PENDIENTES):
    if os.path.exists(ruta):
        with open(ruta, encoding="utf-8") as f:
            return json.load(f)
    return {}


def solicitudes_batch(datos, refs, modelo, max_tokens=16000, con_errores=False):
    """Lista de {custom_id, params} para la Batch API; con_errores añade el error del primer intento (reintento)."""
    g = GeneradorAPI(modelo, cliente=False, max_tokens=max_tokens)
    previos = _errores_previos() if con_errores else {}
    return [{"custom_id": custom_id(r), "params": g.params(solicitud(datos, r), previos.get(r))} for r in refs]


def batch_export(datos, refs, ruta, modelo, max_tokens=16000, con_errores=False):
    with open(ruta, "w", encoding="utf-8") as f:
        for x in solicitudes_batch(datos, refs, modelo, max_tokens, con_errores):
            f.write(json.dumps(x, ensure_ascii=False) + "\n")


def batch_enviar(datos, refs, modelo, cliente=None, max_tokens=16000, con_errores=False):
    """Crea el lote en la Batch API y devuelve su id. Un lote por llamada (máx. 100 000 solicitudes / 256 MB)."""
    cliente = cliente or cliente_anthropic()
    lote = cliente.messages.batches.create(requests=solicitudes_batch(datos, refs, modelo, max_tokens, con_errores))
    return lote.id


def batch_estado(lote_id, cliente=None):
    lote = (cliente or cliente_anthropic()).messages.batches.retrieve(lote_id)
    return {"estado": lote.processing_status, "conteos": lote.request_counts.model_dump()
            if hasattr(lote.request_counts, "model_dump") else dict(lote.request_counts)}


def batch_traer(lote_id, ruta, cliente=None):
    """Descarga los resultados del lote a un JSONL (uno por solicitud)."""
    cliente = cliente or cliente_anthropic()
    n = 0
    with open(ruta, "w", encoding="utf-8") as f:
        for item in cliente.messages.batches.results(lote_id):
            d = item.model_dump() if hasattr(item, "model_dump") else item
            f.write(json.dumps(d, ensure_ascii=False) + "\n")
            n += 1
    return n


def batch_import(datos, ruta, modelo, cache=None, log=ERRORES, uso=None, pendientes=PENDIENTES):
    """Lee los resultados de la Batch API (JSONL), registra los tokens reales y pasa cada respuesta por la misma
    validación y caché. Los que fallan quedan en `pendientes` con su error para un único reintento."""
    cache = cache or Cache(modelo)
    res = {"ok": 0, "error": 0}
    fallos = {}
    with open(ruta, encoding="utf-8") as fh:
        for linea in fh:
            r = json.loads(linea)
            ref = ref_de_custom_id(r["custom_id"])
            try:
                msg = r["result"]["message"]
                registrar_uso(ref, modelo, msg.get("usage"), "batch", uso)
                cards = _extraer_json("".join(b["text"] for b in msg["content"] if b.get("type") == "text"))
                err = validar(cards, solicitud(datos, ref))
            except Exception as e:  # resultado no exitoso o JSON roto
                err = ["resultado no utilizable: %s" % e]
            if err:
                registrar_error(ref, modelo, "batch", err, log)
                fallos[ref] = err
                res["error"] += 1
            else:
                cache.put(ref, cards)
                res["ok"] += 1
    if pendientes and fallos:
        os.makedirs(os.path.dirname(pendientes), exist_ok=True)
        with open(pendientes, "w", encoding="utf-8") as f:
            json.dump(fallos, f, ensure_ascii=False, indent=1)
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


SWORDTEXT_FUENTE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "swordtext.cc")
SWORDTEXT_BIN = os.path.join(RAIZ, ".cache", "swordtext")


def swordtext_bin():
    """Compila (una vez) tools/tagnt/swordtext.cc contra libsword. Requiere g++ y los headers de SWORD."""
    if (not os.path.exists(SWORDTEXT_BIN)
            or os.path.getmtime(SWORDTEXT_BIN) < os.path.getmtime(SWORDTEXT_FUENTE)):
        os.makedirs(os.path.dirname(SWORDTEXT_BIN), exist_ok=True)
        subprocess.run(["g++", "-O2", "-I/usr/include/sword", "-o", SWORDTEXT_BIN, SWORDTEXT_FUENTE, "-lsword"],
                       check=True)
    return SWORDTEXT_BIN


def refs_kjv_nt():
    """Todas las refs OSIS del NT en numeración KJV (la de SpaRV)."""
    out = subprocess.run(["mod2imp", "SpaRV"], capture_output=True, check=True).stdout.decode("utf-8", "replace")
    refs = []
    for linea in out.splitlines():
        m = re.match(r"\$\$\$(.+) (\d+):(\d+)$", linea)
        o = alinear.SWORD_A_OSIS.get(m[1]) if m else None
        if o and int(m[2]) > 0 and int(m[3]) > 0:
            refs.append("%s.%d.%d" % (o, int(m[2]), int(m[3])))
    return refs


def textos_versiones():
    """{(osis, cap, ver) en numeración estándar KJV: [{version, texto, aviso?}]} de SpaRV y TorresAmat.
    El texto sale de la biblioteca SWORD (filtro de texto plano, sin notas) y TorresAmat (Vulg) se mapea a KJV
    con el mapeo de SWORD. Nombre de versión EXACTO del módulo. `aviso` marca versículos con restos de OCR."""
    refs = refs_kjv_nt()
    entrada = ("\n".join(refs) + "\n").encode("utf-8")
    res = {}
    for modulo, conf in VERSIONES:
        ruta = os.path.expanduser("~/.sword/mods.d/%s.conf" % conf)
        with open(ruta, encoding="utf-8") as fh:
            desc = re.search(r"^Description=(.*)$", fh.read(), re.M)[1].strip()
        out = subprocess.run([swordtext_bin(), modulo], input=entrada, capture_output=True, check=True)
        for linea in out.stdout.decode("utf-8").splitlines():
            ref, _, texto = linea.split("\t", 2) if linea.count("\t") >= 2 else (linea, "", "")
            texto = re.sub(r"\s+", " ", texto).strip()
            if not texto:
                continue
            o, c, v = ref.split(".")
            e = {"version": desc, "texto": texto}
            if RE_OCR.search(texto):
                e["aviso"] = AVISO_OCR
            res.setdefault((o, int(c), int(v)), []).append(e)
    return res


def estadistica_ocr(textos=None):
    """(versículos con TorresAmat, con marca de OCR) sobre el NT en numeración estándar."""
    textos = textos or textos_versiones()
    con = [e for lista in textos.values() for e in lista if "Torres" in e["version"]]
    return len(con), sum(1 for e in con if "aviso" in e)


def ensamblar(datos, refs, modelo, cache=None, salida_dir=None):
    cache = cache or Cache(modelo)
    salida = {}
    for ref in refs:
        cards = cache.get(ref)
        if cards is None:
            continue
        sol = solicitud(datos, ref)
        o, c, v = ref.split(".")
        al = {x[0]: x for x in datos.al[ref]}
        std_verso = datos.std_refs(ref)[0]
        proc = cache.meta(ref)
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
                    f["lectura_tisch_propia"] = bool(w["lectura_tagnt"].get("propia_de_tisch"))
                    f.pop("variante", None); f.pop("ausente_en", None); f.pop("no_en_na28", None)
            else:
                f["tagnt"], f["revisar_tagnt"] = None, True
            f["ref_estandar"] = datos.ref_estandar_clave(clave) or std_verso
            # las citas ya no van en la ficha: están en la tabla `citas` (data/citas/, por ref_estandar)
            f["generador"] = "manual" if modelo.startswith("manual") else "api"
            f["modelo"] = modelo
            f["prompt_hash"] = proc["prompt_hash"]
            f["generado_en"] = proc["generado"]
            salida.setdefault("%s.%02d" % (o, int(c)), []).append(f)
    destino = salida_dir or SALIDA
    os.makedirs(destino, exist_ok=True)
    for nombre, lista in salida.items():
        with open(os.path.join(destino, nombre + ".json"), "w", encoding="utf-8") as fh:
            json.dump(lista, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
    return {n: len(l) for n, l in salida.items()}


CITAS = os.path.join(RAIZ, "data", "citas", "citas_nt.json")


def exportar_citas(ruta=CITAS, textos=None):
    """Citas de SpaRV y TorresAmat por ref estándar (KJV) para la tabla `citas` de fichas.sqlite.
    TorresAmat se guarda con ocr_sospechoso=1 en los versículos con restos de OCR (la app no la muestra ahí)."""
    textos = textos or textos_versiones()
    filas = []
    for (o, c, v), lista in sorted(textos.items()):
        for e in lista:
            filas.append({"ref_estandar": "%s.%d.%d" % (o, c, v), "version": e["version"], "texto": e["texto"],
                          "ocr_sospechoso": 1 if "aviso" in e else 0})
    filas.sort(key=lambda x: (x["ref_estandar"], x["version"]))
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    with open(ruta, "w", encoding="utf-8") as f:
        json.dump(filas, f, ensure_ascii=False, indent=0)
        f.write("\n")
    return len(filas)


# ---------------------------------------------------------------- CLI
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("orden", choices=["solicitud", "estado", "cargar", "generar", "batch-export", "batch-import",
                                      "batch-enviar", "batch-estado", "batch-traer", "ensamblar", "exportar-citas"])
    ap.add_argument("--reintento", action="store_true", help="batch: incluir el error del primer intento")
    ap.add_argument("args", nargs="*")
    ap.add_argument("--generador", choices=["manual", "api"], default="manual")
    ap.add_argument("--modelo", default=None)
    a = ap.parse_args(argv)
    datos = Datos()
    modelo = a.modelo or (MODELO_MANUAL if a.generador == "manual" else MODELO_API)
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
    elif a.orden in ("batch-export", "batch-enviar"):
        modelo = a.modelo or MODELO_API
        refs = [r for r in datos.versiculos(a.args[0]) if Cache(modelo).get(r) is None]
        if a.reintento:
            refs = [r for r in refs if r in _errores_previos()]
        if a.orden == "batch-export":
            batch_export(datos, refs, a.args[1], modelo, con_errores=a.reintento)
            print("%d solicitudes en %s" % (len(refs), a.args[1]))
        else:
            lote = batch_enviar(datos, refs, modelo, con_errores=a.reintento)
            print("lote %s con %d solicitudes (modelo %s, prompt %s)" % (lote, len(refs), modelo, prompt_hash()))
    elif a.orden == "batch-estado":
        print(json.dumps(batch_estado(a.args[0]), ensure_ascii=False))
    elif a.orden == "batch-traer":
        modelo = a.modelo or MODELO_API
        n = batch_traer(a.args[0], a.args[1])
        print("%d resultados en %s" % (n, a.args[1]))
        print(batch_import(datos, a.args[1], modelo))
    elif a.orden == "batch-import":
        print(batch_import(datos, a.args[0], modelo))
    elif a.orden == "exportar-citas":
        print(exportar_citas(), "citas en", CITAS)
    elif a.orden == "ensamblar":
        print(ensamblar(datos, datos.versiculos(a.args[0]), modelo))
    return 0


if __name__ == "__main__":
    sys.exit(main())
