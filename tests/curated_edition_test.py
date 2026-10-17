"""Exercise curated project creation through the public command, offline."""
import copy
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest

COMMAND = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class CuratedEditionTest(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory(prefix="elim-curated-test-")
        self.addCleanup(self.workspace.cleanup)
        self.directory = Path(self.workspace.name)
        self.source = self.directory / "SpaRV.sqlite"
        self.output = self.directory / "StudyFixture.sqlite"
        with sqlite3.connect(self.source) as db:
            db.executescript((ROOT / "tests/fixtures/sqlite/test-bible.sql").read_text())
            db.execute("UPDATE metadata SET value='SpaRV' WHERE key='module_id'")
            db.executemany("INSERT INTO metadata VALUES(?,?)", [
                ("license", "Public Domain"), ("source", "Eligible source fixture")])
        self.before = digest(self.source)
        self.plan = {
            "format": "biblia-elim-curated-edition-1",
            "project_id": "StudyFixture", "name": "Biblia de prueba",
            "source_id": "SpaRV", "source_sha256": self.before,
            "passages": [{"book": "Gen", "chapter": 1, "verse": 1,
                          "action": "revise", "expected_base": "In the beginning God created.",
                          "text": "Redacción modificada: creación.", "rationale": "Nota original.",
                          "evidence": "https://example.org/source", "confidence": "Media"}],
            "book_notes": [{"book": "Gen", "comment": "Borrador de estudio."}],
            "glossary": [{"source_term": "λόγος", "target_term": "palabra", "note": "Por contexto."}],
        }

    def run_plan(self, plan=None):
        path = self.directory / "plan.json"
        path.write_text(json.dumps(self.plan if plan is None else plan, ensure_ascii=False))
        return subprocess.run([str(COMMAND), str(self.source), str(self.output), str(path)],
                              text=True, capture_output=True, timeout=30)

    def assert_atomic_failure(self, plan):
        result = self.run_plan(plan)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.directory.glob("*.build-*")), [])
        self.assertEqual(digest(self.source), self.before)

    def test_revisions_and_provenance_persist_without_touching_source(self):
        result = self.run_plan()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(digest(self.source), self.before)
        with sqlite3.connect(self.output) as db:
            self.assertEqual(db.execute("PRAGMA integrity_check").fetchone()[0], "ok")
            self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0], 1)
            self.assertEqual(db.execute("SELECT count(*) FROM verses").fetchone()[0], 5)
            self.assertEqual(dict(db.execute("SELECT key,value FROM metadata"))["source_fingerprint"], self.before)
            self.assertEqual(db.execute("SELECT text FROM verses WHERE book_id=1").fetchone()[0], self.plan["passages"][0]["text"])
            self.assertEqual(db.execute("SELECT old_text,new_text FROM verse_revisions").fetchone(),
                             (self.plan["passages"][0]["expected_base"], self.plan["passages"][0]["text"]))
            self.assertEqual(db.execute("SELECT status,evidence FROM editorial_decisions").fetchone(),
                             ("Pendiente de revisión", "https://example.org/source"))
            self.assertEqual(db.execute("SELECT target_term FROM edition_glossary").fetchone()[0], "palabra")
            self.assertEqual(db.execute("SELECT count(*) FROM verses_fts WHERE verses_fts MATCH 'modificada'").fetchone()[0], 1)
        before = digest(self.output)
        self.assertNotEqual(self.run_plan().returncode, 0)
        self.assertEqual(digest(self.output), before)

    def test_custom_comment_keeps_editorial_presentation(self):
        self.plan["passages"][0]["comment"] = "Título\nBajada.\n\nCuerpo.\n\nFuente: https://example.org/source"
        result = self.run_plan()
        self.assertEqual(result.returncode, 0, result.stderr)
        with sqlite3.connect(self.output) as db:
            self.assertEqual(db.execute("SELECT comment FROM verse_comments").fetchone()[0],
                             self.plan["passages"][0]["comment"])
            self.assertEqual(db.execute("SELECT evidence FROM editorial_decisions").fetchone()[0],
                             "https://example.org/source")

    def test_proposal_keeps_base_wording(self):
        self.plan["passages"][0]["action"] = "propose"
        self.assertEqual(self.run_plan().returncode, 0)
        with sqlite3.connect(self.output) as db:
            self.assertEqual(db.execute("SELECT text FROM verses WHERE book_id=1").fetchone()[0],
                             self.plan["passages"][0]["expected_base"])
            self.assertEqual(db.execute("SELECT count(*) FROM verse_revisions").fetchone()[0], 0)
            self.assertEqual(db.execute("SELECT proposal FROM editorial_decisions").fetchone()[0],
                             self.plan["passages"][0]["text"])

    def test_invalid_reference_rolls_back_whole_project(self):
        plan = copy.deepcopy(self.plan)
        bad = copy.deepcopy(plan["passages"][0]); bad["chapter"] = 99
        plan["passages"].append(bad)
        self.assert_atomic_failure(plan)

    def test_wrong_source_and_wrong_base_are_rejected(self):
        for field in ("source_sha256", "source_id"):
            plan = copy.deepcopy(self.plan); plan[field] = "wrong"
            self.assert_atomic_failure(plan)
        plan = copy.deepcopy(self.plan)
        plan["passages"][0]["expected_base"] = "Not the source wording"
        self.assert_atomic_failure(plan)

    def test_duplicate_or_malformed_passages_are_rejected(self):
        plan = copy.deepcopy(self.plan); plan["passages"] *= 2
        self.assert_atomic_failure(plan)
        plan = copy.deepcopy(self.plan); plan["passages"][0]["verse"] = "1"
        self.assert_atomic_failure(plan)
        plan = copy.deepcopy(self.plan); plan["passages"][0]["action"] = "accept-automatically"
        self.assert_atomic_failure(plan)

    def test_project_clone_retains_original_provenance_and_history(self):
        self.assertEqual(self.run_plan().returncode, 0)
        self.source = self.output
        before = digest(self.source)
        self.output = self.directory / "Renamed.sqlite"
        self.plan.update(project_id="Renamed", name="Renamed edition")
        self.plan["passages"][0]["expected_current"] = self.plan["passages"][0]["text"]
        result = self.run_plan()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(digest(self.source), before)
        with sqlite3.connect(self.output) as db:
            metadata = dict(db.execute("SELECT key,value FROM metadata"))
            self.assertEqual(metadata["source_module_id"], "SpaRV")
            self.assertEqual(metadata["source_fingerprint"], self.before)
            self.assertEqual(db.execute("SELECT count(*) FROM verse_revisions").fetchone()[0], 1)
            self.assertEqual(db.execute("SELECT text FROM edition_base_verses WHERE book_id=1").fetchone()[0],
                             self.plan["passages"][0]["expected_base"])
            self.assertEqual(db.execute("SELECT evidence FROM editorial_decisions").fetchone()[0],
                             "https://example.org/source")

    def test_english_comments_and_language_validation(self):
        with sqlite3.connect(self.source) as db:
            db.execute("UPDATE metadata SET value='KJV' WHERE key='module_id'")
            db.execute("UPDATE metadata SET value='en' WHERE key='language'")
            db.execute("UPDATE metadata SET value='GPL' WHERE key='license'")
        self.before = digest(self.source)
        self.plan.update(source_id="KJV", source_sha256=self.before, language="en", name="Manning ENG")
        result = self.run_plan()
        self.assertEqual(result.returncode, 0, result.stderr)
        with sqlite3.connect(self.output) as db:
            note = db.execute("SELECT comment FROM verse_comments").fetchone()[0]
            self.assertIn("Manning ENG", note)
            self.assertIn("Sources:\nhttps://example.org/source", note)
            self.assertIn("Confidence: Medium", note)
            self.assertNotIn("Base RV1909", note)
            self.assertEqual(dict(db.execute("SELECT key,value FROM metadata"))["license"], "GPL")
        self.output = self.directory / "WrongLanguage.sqlite"
        self.plan["language"] = "es"
        self.assert_atomic_failure(self.plan)

    def test_bilingual_plans_share_sources_but_keep_language_and_base(self):
        plans = [json.loads((ROOT / f"doc/manning/editorial-plan-{lang}.json").read_text())
                 for lang in ("es", "en")]
        source_sets = []
        for plan, lang, base, name in zip(plans, ("es", "en"), ("SpaRV", "KJV"),
                                         ("Manning ESP", "Manning ENG")):
            self.assertEqual(plan["language"], lang)
            self.assertEqual(plan["source_id"], base)
            self.assertEqual(plan["name"], name)
            self.assertEqual(len(plan["passages"]), 81)
            self.assertEqual(len(plan["book_notes"]), 66)
            self.assertEqual(len(plan["glossary"]), 9)
            source_sets.append({p["editorial_id"]: p["evidence"] for p in plan["passages"]})
            self.assertTrue(all(p["expected_base"] and p["rationale"] for p in plan["passages"]))
            self.assertFalse(any("expected_current" in p for p in plan["passages"]))
            self.assertEqual(sum(p["action"] == "revise" for p in plan["passages"]), 11)
        self.assertEqual(source_sets[0], source_sets[1])
        genesis = next(p for p in plans[0]["passages"] if p["editorial_id"] == "Gen.1.1")
        self.assertEqual(genesis["action"], "annotate")
        self.assertEqual(genesis["text"], "")
        self.assertEqual(genesis["comment"], genesis["rationale"])
        self.assertIn("Alfa y la Omega", genesis["comment"])
        self.assertIn("Fuente: Gary Manning Jr.", genesis["comment"])
        self.assertTrue(genesis["comment"].endswith(genesis["evidence"]))
        self.assertNotEqual(plans[0]["passages"][0]["text"], plans[1]["passages"][0]["text"])

    def test_every_manning_note_is_a_standalone_bilingual_comment(self):
        for lang, source_label in (("es", "Fuente:"), ("en", "Source:")):
            plan = json.loads((ROOT / f"doc/manning/editorial-plan-{lang}.json").read_text())
            titles = set()
            for passage in plan["passages"]:
                ref = passage["editorial_id"]
                with self.subTest(language=lang, reference=ref):
                    title, lead, body = (passage[k] for k in ("note_title", "note_lead", "note_body"))
                    self.assertTrue(0 < len(title) <= 120)
                    self.assertTrue(0 < len(lead) <= 300)
                    self.assertNotIn("\n", title)
                    self.assertNotIn("\n", lead)
                    self.assertNotIn(title, titles)
                    titles.add(title)
                    self.assertGreaterEqual(len(body.split()), 90)
                    self.assertGreaterEqual(len(body.split("\n\n")), 2)
                    self.assertEqual(passage["comment"], passage["rationale"])
                    self.assertTrue(passage["comment"].startswith(title + "\n" + lead + "\n\n" + body))
                    self.assertIn("\n\n" + source_label, passage["comment"])
                    self.assertLessEqual(len(passage["comment"].split()), 270)
                    for url in passage["evidence"].splitlines():
                        self.assertIn(url, passage["comment"])
                    self.assertTrue(passage["editorial_audit"])
                    for phrase in ("en el artículo", "lee el artículo", "read the article", "do not replace"):
                        self.assertNotIn(phrase, body.lower())

    def test_manning_summaries_preserve_context_and_concrete_arguments(self):
        # Guard against replacing the actual argument with a generic moral.
        anchors = {
            "1Sam.7.12": {
                "es": ("1 Samuel 4", "arca", "Ofni", "ídolos", "confiesa", "intercede", "7:3–10", "bautismo"),
                "en": ("1 Samuel 4", "ark", "Hophni", "idols", "confess", "intercedes", "7:3–10", "baptism"),
            },
            "John.4.9": {"es": ("dativo", "Polibio", "flota", "ayuda"),
                         "en": ("dative", "Polybius", "fleets", "help")},
            "John.19.30": {"es": ("315", "17", "τετελώνηται", "19:28–30", "17:4"),
                           "en": ("315", "17", "tetelōnētai", "19:28–30", "17:4")},
            "John.21.17": {"es": ("la tercera vez", "negaciones", "Juan 18"),
                           "en": ("the third time", "denials", "John 18")},
            "Rom.7.14": {"es": ("7:13", "6:18,22", "8:2", "8:9", "debatida"),
                         "en": ("7:13", "6:18,22", "8:2", "8:9", "debated")},
            "Gen.1.1": {"es": ("video", "entre «Dios»", "objeto directo", "2:6", "10:8"),
                        "en": ("video", "between «God»", "direct object", "2:6", "10:8")},
            "1Tim.2.12": {"es": ("Trifón", "Antíloco", "lagunas", "sintaxis"),
                          "en": ("Tryphon", "Antilochos", "gaps", "syntax")},
            "Prov.31.6": {"es": ("31:4–5", "31:8–9", "Lemuel", "rey"),
                          "en": ("31:4–5", "31:8–9", "Lemuel", "king")},
            "Acts.20.10": {"es": ("20:9", "20:12", "2:42–47", "Macedonia"),
                           "en": ("20:9", "20:12", "2:42–47", "Macedonia")},
        }
        for lang in ("es", "en"):
            plan = json.loads((ROOT / f"doc/manning/editorial-plan-{lang}.json").read_text())
            passages = {p["editorial_id"]: p for p in plan["passages"]}
            for p in passages.values():
                self.assertEqual(p["comment_revision"], "MANNING-108")
                self.assertTrue(p["comment_review_history"][-1]["comment"])
            for ref, languages in anchors.items():
                with self.subTest(language=lang, reference=ref):
                    for fact in languages[lang]:
                        self.assertIn(fact, passages[ref]["note_body"])

    def test_manning_plan_has_reviewable_sources_and_coverage(self):
        plan = json.loads((ROOT / "doc/manning/editorial-plan.json").read_text())
        inventory = json.loads((ROOT / "doc/manning/sources.json").read_text())
        urls = {s["url"] for s in inventory["sources"]}
        self.assertEqual(len([s for s in inventory["sources"] if s["id"].startswith("biola-")]), 44)
        self.assertEqual(len([s for s in inventory["sources"] if s["id"].startswith("eutychus-")]), 204)
        self.assertEqual(len(plan["book_notes"]), 66)
        self.assertIn("no es autor", plan["edition_notice"])
        references = set()
        for p in plan["passages"]:
            ref = (p["book"], p["chapter"], p["verse"])
            self.assertNotIn(ref, references); references.add(ref)
            self.assertTrue(p["rationale"] and p["evidence"])
            self.assertTrue(set(p["evidence"].splitlines()) <= urls)
        john49 = next(p for p in plan["passages"] if (p["book"],p["chapter"],p["verse"]) == ("John",4,9))
        self.assertIn("no piden ayuda", john49["text"])
        john1930 = next(p for p in plan["passages"] if (p["book"],p["chapter"],p["verse"]) == ("John",19,30))
        self.assertIn("Está cumplido", john1930["text"])
        self.assertNotIn("deuda", john1930["text"])


if __name__ == "__main__":
    unittest.main()
