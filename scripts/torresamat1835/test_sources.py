"""
TORRES-1835-SOURCE-102: los seis tomos, registrados y verificables.

    python3 test_sources.py

Sin red y sin tocar ~/.sword. Trabaja sobre copias temporales del
manifest, así que no puede alterar el del repositorio.
"""
import hashlib
import json
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import register_source as rs
from register_source import SourceError

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
MANIFEST = os.path.join(ROOT, "data", "torresamat1835", "source_manifest.json")

PAYLOAD = b"un tomo de prueba, no el facsimil\n"
DIGEST = hashlib.sha256(PAYLOAD).hexdigest()


def _manifest():
    with open(MANIFEST, encoding="utf-8") as handle:
        return json.load(handle)


class Sandbox:
    """Manifest y caché desechables: los tests nunca tocan el repo."""

    def __enter__(self):
        self.tmp = tempfile.mkdtemp()
        self.manifest = os.path.join(self.tmp, "manifest.json")
        shutil.copy2(MANIFEST, self.manifest)
        self.cache = os.path.join(self.tmp, "cache")
        self.artifact = os.path.join(self.tmp, "tomo.pdf")
        with open(self.artifact, "wb") as handle:
            handle.write(PAYLOAD)
        return self

    def __exit__(self, *exc):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def load(self):
        with open(self.manifest, encoding="utf-8") as handle:
            return json.load(handle)


# ---- A. Esquema del manifest ------------------------------------------
def test_manifest_has_six_volumes():
    data = _manifest()
    assert data["manifest_version"] >= 2
    volumes = data["volumes"]
    assert len(volumes) == 6, len(volumes)
    assert [v["volume"] for v in volumes] == [1, 2, 3, 4, 5, 6]
    assert len({v["volume"] for v in volumes}) == 6

    assert data["edition"]["volumes_total"] == 6
    assert data["edition"]["publisher"].endswith("Miguel de Burgos")
    assert data["edition"]["versification"] == "Vulg"
    assert "1832-1835" in data["edition"]["years"]

    for volume in volumes:
        for field in ("volume", "metadata_status", "status", "acquisition",
                      "witnesses", "files", "carries_biblical_text"):
            assert field in volume, (volume["volume"], field)
        assert volume["metadata_status"] in ("verified", "incomplete")
        assert volume["status"] in ("verified", "available",
                                    "manual_download_required",
                                    "metadata_incomplete", "unavailable")
        assert volume["witnesses"], volume["volume"]


# ---- B. Derechos por tomo ---------------------------------------------
def test_every_volume_states_its_rights():
    for volume in _manifest()["volumes"]:
        for witness in volume["witnesses"]:
            rights = witness.get("rights")
            assert rights, (volume["volume"], witness.get("id"))
            for field in ("rights_statement", "attribution_text",
                          "redistribution_allowed"):
                assert rights.get(field), (witness.get("id"), field)
            # Nada ambiguo: la redistribución se dice con una de estas.
            assert rights["redistribution_allowed"] in (
                "yes", "no", "yes-with-attribution", "not-established")


# ---- C. Un tomo sin adquirir no puede decir que está verificado --------
def test_missing_checksum_is_not_verified():
    for volume in _manifest()["volumes"]:
        for entry in volume.get("files", []):
            if entry.get("sha256") is None:
                assert entry.get("verified") is False, entry["filename"]
            else:
                assert len(entry["sha256"]) == 64, entry["filename"]
        if not volume.get("files"):
            assert volume["status"] != "verified", volume["volume"]


# ---- D. Registrar un artefacto ----------------------------------------
def test_register_records_only_with_flag():
    with Sandbox() as box:
        done = rs.register(1, box.artifact, manifest_path=box.manifest,
                           cache=box.cache, record=True, filename="tomo1.pdf")
        assert done["sha256"] == DIGEST
        assert os.path.isfile(os.path.join(box.cache, "tomo1.pdf"))

        volume = next(v for v in box.load()["volumes"] if v["volume"] == 1)
        entry = next(f for f in volume["files"] if f["filename"] == "tomo1.pdf")
        assert entry["sha256"] == DIGEST
        assert entry["verified"] is True
        assert volume["status"] == "verified"

        # Y una segunda verificación, ya sin --record, pasa.
        again = rs.register(1, box.artifact, manifest_path=box.manifest,
                            cache=box.cache, filename="tomo1.pdf")
        assert again["sha256"] == DIGEST
        assert again["recorded"] is False


# ---- E. Un artefacto que no es el suyo ---------------------------------
def test_unknown_artifact_is_refused():
    with Sandbox() as box:
        try:
            rs.register(1, box.artifact, manifest_path=box.manifest,
                        cache=box.cache, filename="cualquier-cosa.pdf")
        except SourceError as exc:
            assert "no describe" in str(exc)
        else:
            raise AssertionError("un fichero no descrito no puede registrarse")

        # Y un tomo que no existe tampoco.
        try:
            rs.register(9, box.artifact, manifest_path=box.manifest,
                        cache=box.cache, record=True)
        except SourceError:
            pass
        else:
            raise AssertionError("no hay tomo 9")


# ---- F. Mismo nombre, otros bytes --------------------------------------
def test_changed_artifact_is_refused():
    with Sandbox() as box:
        rs.register(1, box.artifact, manifest_path=box.manifest,
                    cache=box.cache, record=True, filename="tomo1.pdf")
        with open(box.artifact, "wb") as handle:
            handle.write(PAYLOAD + b"pero cambiado\n")
        try:
            rs.register(1, box.artifact, manifest_path=box.manifest,
                        cache=box.cache, filename="tomo1.pdf")
        except SourceError as exc:
            assert "ha cambiado" in str(exc) or "no es el mismo artefacto" in str(exc)
        else:
            raise AssertionError("bytes distintos con el mismo nombre: rechazo")


# ---- G. Nunca se registra en silencio ----------------------------------
def test_no_silent_record():
    with Sandbox() as box:
        before = box.load()
        try:
            rs.register(1, box.artifact, manifest_path=box.manifest,
                        cache=box.cache, filename="tomo1.pdf")
        except SourceError as exc:
            assert "--record" in str(exc)
        else:
            raise AssertionError("sin --record no puede aceptarse un hash nuevo")
        assert box.load() == before, "el manifest no puede cambiar sin --record"


# ---- H. Nada grande vive en Git ----------------------------------------
#: Los registros de revisión humana y las tablas de medidas que los
#: acompañan. Crecen con el trabajo hecho, son texto reproducible y no
#: son assets de la fuente -- ni escaneos ni volcados de OCR --, así que
#: tienen su propio techo. Lo que el guarda persigue sigue prohibido en
#: todos los demás ficheros.
_REVIEW_LEDGERS = ("chapter_image_reviews.json", "verse_boundary_reviews.json",
                   "a_glyph_pixel_features.json")


def test_sources_do_not_live_in_git():
    # Metadata versionada sí; assets de la fuente no. Lo que no puede
    # haber aquí son escaneos, OCR masivo ni nada pesado: eso vive en la
    # caché de build/, ignorada por Git.
    data_dir = os.path.join(ROOT, "data", "torresamat1835")
    listing = sorted(os.listdir(data_dir))
    assert "source_manifest.json" in listing
    for name in listing:
        assert name.endswith(".json"), name
        # El registro de revisiones crece una entrada por cada decisión
        # humana, y cada entrada lleva su evidencia: qué se vio en la
        # plana y por qué. Eso es exactamente lo que debe estar
        # versionado, así que tiene su propio techo. Lo que el guarda
        # persigue --escaneos, volcados de OCR, cualquier cosa pesada de
        # la fuente-- sigue prohibido en todos los demás ficheros.
        # Los dos ledgers de revisión crecen una entrada por decisión
        # humana y cada entrada lleva su evidencia escrita: es justo lo
        # que debe estar versionado, y por eso tienen techo propio.
        cap = 768 if name in _REVIEW_LEDGERS else 256
        assert os.path.getsize(os.path.join(data_dir, name)) < cap * 1024, name

    heavy = (".pdf", ".jp2", ".djvu", ".tif", ".tiff", ".zip", ".gz", ".xml")
    for base in (data_dir, DIR):
        for root, _dirs, files in os.walk(base):
            if "__pycache__" in root:
                continue
            for name in files:
                assert not name.lower().endswith(heavy), os.path.join(root, name)
                size = os.path.getsize(os.path.join(root, name))
                cap = 768 if name in _REVIEW_LEDGERS else 256
                assert size < cap * 1024, (name, size)

    cache = _manifest()["cache"]["default"]
    assert cache.startswith("build/"), cache
    # build/ está ignorado por Git.
    with open(os.path.join(ROOT, ".gitignore"), encoding="utf-8") as handle:
        ignored = handle.read()
    assert any(line.strip().rstrip("/") == "build"
               for line in ignored.splitlines()), ignored[:200]
    # Y ninguna RUTA del manifest apunta al home del usuario ni a
    # ~/.sword. Se miran los valores que son rutas, no la prosa: la
    # política de caché menciona ~/.sword justo para decir que no se
    # toca, y esa frase debe poder escribirse.
    def paths(node):
        if isinstance(node, dict):
            for key, value in node.items():
                yield from paths(value)
        elif isinstance(node, list):
            for value in node:
                yield from paths(value)
        elif isinstance(node, str):
            candidate = node.strip()
            if (candidate.startswith(("/", "~", "file://"))
                    or candidate.startswith("build/")):
                yield candidate

    for candidate in paths(_manifest()):
        assert ".sword" not in candidate, candidate
        assert not candidate.startswith("/home/"), candidate
        assert not candidate.startswith("~"), candidate


# ---- I. Determinismo ----------------------------------------------------
def test_verification_is_deterministic():
    with Sandbox() as box:
        rs.register(1, box.artifact, manifest_path=box.manifest,
                    cache=box.cache, record=True, filename="tomo1.pdf")
        results = [rs.register(1, box.artifact, manifest_path=box.manifest,
                               cache=box.cache, filename="tomo1.pdf")["sha256"]
                   for _ in range(3)]
        assert len(set(results)) == 1
        assert results[0] == DIGEST


# ---- J. El status no sale a la red -------------------------------------
def test_status_is_offline_and_reports_six():
    with Sandbox() as box:
        _manifest_data, target, rows = rs.status(box.manifest, cache=box.cache)
        assert len(rows) == 6
        assert [r["volume"] for r in rows] == [1, 2, 3, 4, 5, 6]
        assert box.tmp in target
        for row in rows:
            # Las tres cuentas de fichero son coherentes entre sí:
            # verificado implica adquirido, y adquirido implica descrito.
            assert row["files_verified"] <= row["files_cached"]
            assert row["files_cached"] <= row["files_described"]
            assert row["files_with_checksum"] <= row["files_described"]
    # Ningún módulo de este directorio importa urllib salvo el descargador.
    for name in ("register_source.py", "model.py", "parser.py", "osis_out.py"):
        with open(os.path.join(DIR, name), encoding="utf-8") as handle:
            assert "urllib" not in handle.read(), name


# ---- K. Semántica del recuento ----------------------------------------
def test_status_counts_are_not_conflated():
    """Metadata y artefactos son cuentas distintas.

    El fallo que esto ataja: el resumen decía «artefactos verificados
    0/6» teniendo un artefacto con su sha256 registrado, porque contaba
    el estado del tomo en vez de los ficheros.
    """
    with Sandbox() as box:
        manifest, _target, rows = rs.status(box.manifest, cache=box.cache)
        summary = rs.summarise(manifest, rows)

        # Las claves existen y son cosas separadas.
        for key in ("edition_metadata", "volume_metadata_verified",
                    "artifacts_acquired", "artifacts_verified",
                    "biblical_corpus_metadata_verified",
                    "biblical_corpus_artifacts_verified",
                    "release_capable_artifacts_verified"):
            assert key in summary, key

        # Nada adquirido en una caché vacía, pase lo que pase con la
        # metadata: es justo la confusión que había.
        assert summary["artifacts_acquired"] == 0
        assert summary["artifacts_verified"] == 0
        assert summary["volume_metadata_verified"] >= 5
        assert summary["edition_metadata"] == "verified"
        assert summary["biblical_corpus_ready"] is False

        # Y en cuanto hay un artefacto de verdad, sube la cuenta de
        # artefactos y sólo esa.
        before = dict(summary)
        rs.register(1, box.artifact, manifest_path=box.manifest,
                    cache=box.cache, record=True, filename="tomo1.pdf")
        manifest, _target, rows = rs.status(box.manifest, cache=box.cache)
        after = rs.summarise(manifest, rows)
        assert after["artifacts_acquired"] == 1
        assert after["artifacts_verified"] == 1
        assert after["volume_metadata_verified"] == before["volume_metadata_verified"]


def test_described_is_not_acquired():
    """Un fichero descrito con checksum no es un fichero en la mano."""
    with Sandbox() as box:
        _m, _t, rows = rs.status(box.manifest, cache=box.cache)
        volume3 = next(r for r in rows if r["volume"] == 3)
        assert volume3["files_described"] >= 1
        assert volume3["files_with_checksum"] >= 1
        assert volume3["files_cached"] == 0
        assert volume3["files_verified"] == 0


# ---- L. Tomo 6: de la edición, pero no del corpus bíblico -------------
def test_volume_six_is_supplementary():
    data = _manifest()
    corpus = data["edition"]["corpus"]
    assert corpus["biblical_corpus_volumes"] == [1, 2, 3, 4, 5]
    assert corpus["supplementary_volumes"] == [6]

    by_number = {v["volume"]: v for v in data["volumes"]}
    # El tomo 6 sigue registrado: pertenece a la edición.
    assert 6 in by_number
    six = by_number[6]
    assert six["corpus_role"] == "supplementary"
    assert six["required_for_biblical_corpus"] is False
    assert six["carries_biblical_text"] is False
    assert six["books_covered"] == []
    assert "notes_dictionary_feature" in six["required_for"]

    for number in (1, 2, 3, 4, 5):
        volume = by_number[number]
        assert volume["corpus_role"] == "biblical_corpus"
        assert volume["required_for_biblical_corpus"] is True

    # Que falte el 6 no puede hacer que el corpus bíblico esté incompleto.
    with Sandbox() as box:
        manifest, _t, rows = rs.status(box.manifest, cache=box.cache)
        summary = rs.summarise(manifest, rows)
        assert summary["biblical_corpus_volumes"] == 5
        assert summary["supplementary_volumes"] == 1
        corpus_rows = [r for r in rows if r["required_for_biblical_corpus"]]
        assert len(corpus_rows) == 5
        assert all(r["volume"] != 6 for r in corpus_rows)


# ---- M. El tomo 2 sigue sin inferirse ----------------------------------
def test_volume_two_is_not_inferred():
    two = next(v for v in _manifest()["volumes"] if v["volume"] == 2)
    assert two["books_covered"] is None
    assert two["metadata_status"] == "incomplete"
    assert two["publication_year"] is None
    assert two["title_page_text"] is None


# ---- N. El testigo de desarrollo no puede acabar en el release ---------
def test_development_witness_cannot_ship():
    data = _manifest()
    roles = set()
    for volume in data["volumes"]:
        for witness in volume["witnesses"]:
            assert witness.get("role") in ("distributable_primary",
                                           "development_witness"), witness["id"]
            roles.add(witness["role"])
            may = witness.get("may_produce_release_artifact")
            assert isinstance(may, bool), witness["id"]
            if witness["role"] == "development_witness":
                assert may is False, witness["id"]
                assert witness["rights"]["redistribution_allowed"] in (
                    "no", "not-established"), witness["id"]
            else:
                assert may is True, witness["id"]
    assert roles == {"distributable_primary", "development_witness"}

    # El volumen 3 de Google: explícitamente prohibido para release.
    three = next(v for v in data["volumes"] if v["volume"] == 3)
    google = next(w for w in three["witnesses"]
                  if w["id"] == "ia-lasagradabiblia01unkngoog")
    assert google["role"] == "development_witness"
    assert google["may_produce_release_artifact"] is False
    assert google["rights"]["redistribution_allowed"] == "no"
    assert "forbidden" in google["rights"]["release_use"]

    # Y sus ficheros arrastran la misma marca, para que no se cuele por ahí.
    for entry in three["files"]:
        if entry.get("witness") == "ia-lasagradabiblia01unkngoog":
            assert entry["may_produce_release_artifact"] is False
            assert entry["witness_role"] == "development_witness"

    # Ningún artefacto apto para release está verificado todavía.
    with Sandbox() as box:
        manifest, _t, rows = rs.status(box.manifest, cache=box.cache)
        assert rs.summarise(manifest, rows)["release_capable_artifacts_verified"] == 0


if __name__ == "__main__":
    failures = 0
    for name, func in sorted(globals().items()):
        if name.startswith("test_") and callable(func):
            try:
                func()
                print(f"ok {name}")
            except AssertionError as exc:
                failures += 1
                print(f"FAIL {name}: {exc}")
    print(f"torresamat1835_sources_failures={failures}")
    sys.exit(1 if failures else 0)
