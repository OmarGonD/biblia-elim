#!/usr/bin/env python3
"""Construye data/fichas.sqlite desde los JSON versionados (reproducible: mismas entradas, mismo archivo).

Fuentes (un solo formato, el de v3):
  data/fichas_v2/*.json   fichas heredadas (generador manual, modelo manual-legacy)
  data/fichas_v3/*.json   fichas nuevas; si coincide la clave (ref_tisch, posicion, strong), gana v3,
                           salvo que la ficha v2 tenga sentido_en_contexto y la v3 no (se conserva la v2)
  data/citas/citas_nt.json  citas de SpaRV y TorresAmat por ref_estandar (tabla `citas`)

El .sqlite no se commitea: lo genera el build (CMake) o este comando:
  python3 tools/construir_fichas_sqlite.py [--output data/fichas.sqlite]
Solo usa la biblioteca estándar. El formato es propio (PRAGMA user_version = 1) y no toca el de módulos bíblicos.
"""
import argparse, glob, hashlib, json, os, sqlite3, sys

RAIZ = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
FUENTES = [os.path.join(RAIZ, "data", "fichas_v2"), os.path.join(RAIZ, "data", "fichas_v3")]
CITAS = os.path.join(RAIZ, "data", "citas", "citas_nt.json")
SALIDA = os.path.join(RAIZ, "data", "fichas.sqlite")
FORMATO = 1

ESQUEMA = """
CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE fichas (
  id INTEGER PRIMARY KEY,
  ref_tisch TEXT NOT NULL,
  posicion INTEGER NOT NULL CHECK (posicion > 0),
  strong TEXT NOT NULL,
  ref_estandar TEXT,
  strong_extendido TEXT,
  ref_tagnt TEXT,
  posicion_tagnt INTEGER,
  forma TEXT NOT NULL,
  lema TEXT,
  glosa_interlineal TEXT NOT NULL,
  rango_semantico TEXT CHECK (rango_semantico IS NULL OR json_valid(rango_semantico)),
  construccion TEXT,
  sentido_en_contexto TEXT,
  matiz TEXT,
  variantes_textuales TEXT,
  notas_traduccion TEXT,
  otros_usos TEXT CHECK (otros_usos IS NULL OR json_valid(otros_usos)),
  morfologia_tagnt TEXT,
  caso_regido TEXT,
  caso_regido_nota TEXT,
  ediciones TEXT CHECK (ediciones IS NULL OR json_valid(ediciones)),
  ausente_en TEXT CHECK (ausente_en IS NULL OR json_valid(ausente_en)),
  lectura_tagnt TEXT CHECK (lectura_tagnt IS NULL OR json_valid(lectura_tagnt)),
  alineacion TEXT,
  nivel_certeza TEXT CHECK (nivel_certeza IS NULL OR nivel_certeza IN ('alto','medio','bajo')),
  revisar_ocurrencia INTEGER NOT NULL DEFAULT 0 CHECK (revisar_ocurrencia IN (0,1)),
  revisar_tagnt INTEGER NOT NULL DEFAULT 0 CHECK (revisar_tagnt IN (0,1)),
  variante INTEGER NOT NULL DEFAULT 0 CHECK (variante IN (0,1)),
  no_en_na28 INTEGER NOT NULL DEFAULT 0 CHECK (no_en_na28 IN (0,1)),
  lectura_propia_tisch INTEGER NOT NULL DEFAULT 0 CHECK (lectura_propia_tisch IN (0,1)),
  generador TEXT NOT NULL CHECK (generador IN ('manual','api')),
  modelo TEXT,
  prompt_hash TEXT,
  generado_en TEXT,
  UNIQUE (ref_tisch, posicion, strong)
);
CREATE INDEX fichas_ref ON fichas (ref_tisch, posicion);
CREATE INDEX fichas_strong ON fichas (strong);
CREATE INDEX fichas_std ON fichas (ref_estandar);
CREATE TABLE citas (
  ref_estandar TEXT NOT NULL,
  version TEXT NOT NULL,
  texto TEXT NOT NULL,
  ocr_sospechoso INTEGER NOT NULL DEFAULT 0 CHECK (ocr_sospechoso IN (0,1)),
  PRIMARY KEY (ref_estandar, version)
);
"""

# claves JSON admitidas -> se rechaza cualquier otra (evita que los datos y la base se desincronicen)
CLAVES = {"ref", "pos_tisch", "strong", "forma", "lema", "glosa_interlineal", "rango_semantico", "construccion",
          "sentido_en_contexto", "matiz", "variantes_textuales", "notas_traduccion", "otros_usos", "nivel_certeza",
          "alineacion", "tagnt", "ref_estandar", "morfologia_tagnt", "dstrong", "strong_tagnt", "ediciones",
          "ausente_en", "variante", "no_en_na28", "caso_regido", "caso_regido_ambiguo", "revisar_ocurrencia",
          "revisar_tagnt", "generador", "modelo", "prompt_hash", "generado_en", "lectura_tagnt",
          "lectura_tisch_propia"}


def _json(v):
    return None if v is None else json.dumps(v, ensure_ascii=False, separators=(",", ":"))


def _fila(f, origen):
    extra = set(f) - CLAVES
    if extra:
        raise ValueError("%s: claves no admitidas en %s: %s" % (origen, f.get("ref"), sorted(extra)))
    ref_tagnt = pos_tagnt = None
    if f.get("tagnt"):
        ref_tagnt, _, p = f["tagnt"].partition("#")
        pos_tagnt = int(p)
    return {
        "ref_tisch": f["ref"], "posicion": f["pos_tisch"], "strong": f["strong"],
        "ref_estandar": f.get("ref_estandar"), "strong_extendido": f.get("dstrong"),
        "ref_tagnt": ref_tagnt, "posicion_tagnt": pos_tagnt, "forma": f["forma"], "lema": f.get("lema"),
        "glosa_interlineal": f["glosa_interlineal"], "rango_semantico": _json(f.get("rango_semantico")),
        "construccion": f.get("construccion"), "sentido_en_contexto": f.get("sentido_en_contexto"),
        "matiz": f.get("matiz"), "variantes_textuales": f.get("variantes_textuales"),
        "notas_traduccion": f.get("notas_traduccion"), "otros_usos": _json(f.get("otros_usos")),
        "morfologia_tagnt": f.get("morfologia_tagnt"), "caso_regido": f.get("caso_regido"),
        "caso_regido_nota": f.get("caso_regido_ambiguo"), "ediciones": _json(f.get("ediciones")),
        "ausente_en": _json(f.get("ausente_en")), "lectura_tagnt": _json(f.get("lectura_tagnt")),
        "alineacion": f.get("alineacion"), "nivel_certeza": f.get("nivel_certeza"),
        "revisar_ocurrencia": int(bool(f.get("revisar_ocurrencia"))), "revisar_tagnt": int(bool(f.get("revisar_tagnt"))),
        "variante": int(bool(f.get("variante"))), "no_en_na28": int(bool(f.get("no_en_na28"))),
        "lectura_propia_tisch": int(bool(f.get("lectura_tisch_propia"))), "generador": f["generador"],
        "modelo": f.get("modelo"), "prompt_hash": f.get("prompt_hash"), "generado_en": f.get("generado_en"),
    }


def _combinar(previa, nueva):
    """La fuente posterior gana, salvo que la anterior sea una ficha con `sentido_en_contexto` y la nueva
    no: entonces se conserva la anterior (más rica) y solo se completan sus huecos con datos de la nueva."""
    if previa is None or not previa["sentido_en_contexto"] or nueva["sentido_en_contexto"]:
        return nueva
    fila = dict(previa)
    for col, v in nueva.items():
        if fila.get(col) in (None, 0) and v not in (None, 0):
            fila[col] = v
    return fila


def leer_fichas(fuentes=None):
    """{(ref_tisch, posicion, strong): fila}; las fuentes posteriores ganan. Devuelve también los archivos leídos."""
    fichas, archivos = {}, []
    for dir_ in fuentes or FUENTES:
        propias = {}
        for ruta in sorted(glob.glob(os.path.join(dir_, "*.json"))):
            archivos.append(ruta)
            with open(ruta, encoding="utf-8") as fh:
                for f in json.load(fh):
                    fila = _fila(f, ruta)
                    k = (fila["ref_tisch"], fila["posicion"], fila["strong"])
                    if k in propias:
                        raise ValueError("clave duplicada dentro de %s: %s" % (dir_, k))
                    propias[k] = fila
        for k, fila in propias.items():
            fichas[k] = _combinar(fichas.get(k), fila)
    return fichas, archivos


def construir(salida=SALIDA, fuentes=None, citas=CITAS):
    fichas, archivos = leer_fichas(fuentes)
    cit = []
    if citas and os.path.exists(citas):
        archivos.append(citas)
        with open(citas, encoding="utf-8") as fh:
            cit = json.load(fh)
    h = hashlib.sha256()
    for ruta in sorted(archivos):
        h.update(os.path.relpath(ruta, RAIZ).encode())
        with open(ruta, "rb") as fh:
            h.update(hashlib.sha256(fh.read()).digest())
    tmp = salida + ".tmp"
    for r in (tmp, tmp + "-journal"):
        if os.path.exists(r):
            os.remove(r)
    os.makedirs(os.path.dirname(os.path.abspath(salida)), exist_ok=True)
    db = sqlite3.connect(tmp)
    db.executescript("PRAGMA page_size=4096; PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF; PRAGMA user_version=%d;" % FORMATO)
    db.executescript(ESQUEMA)
    cols = list(next(iter(fichas.values())).keys()) if fichas else []
    orden = sorted(fichas, key=lambda k: (k[0], k[1], k[2]))
    db.executemany("INSERT INTO fichas (%s) VALUES (%s)" % (",".join(cols), ",".join("?" * len(cols))),
                   [tuple(fichas[k][c] for c in cols) for k in orden])
    db.executemany("INSERT INTO citas VALUES (?,?,?,?)",
                   [(c["ref_estandar"], c["version"], c["texto"], c["ocr_sospechoso"])
                    for c in sorted(cit, key=lambda c: (c["ref_estandar"], c["version"]))])
    meta = [("formato", "fichas-v%d" % FORMATO), ("fuentes_sha256", h.hexdigest()), ("fichas", str(len(fichas))),
            ("citas", str(len(cit))),
            ("atribucion", "Datos del texto griego: STEPBible.org, Tyndale House, Cambridge (CC BY 4.0), "
                           "https://github.com/STEPBible/STEPBible-Data. Cambios: data/sources/tagnt/SOURCE.md"),
            ("citas_versiones", "La Santa Biblia Reina-Valera (1909); La Sagrada Biblia (Torres Amat) — dominio público")]
    db.executemany("INSERT INTO metadata VALUES (?,?)", sorted(meta))
    db.commit()
    db.close()
    os.replace(tmp, salida)
    return len(fichas), len(cit)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--output", default=SALIDA)
    a = ap.parse_args(argv)
    f, c = construir(a.output)
    print("%s: %d fichas, %d citas" % (a.output, f, c))


if __name__ == "__main__":
    main()
