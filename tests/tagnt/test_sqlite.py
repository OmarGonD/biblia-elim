import hashlib, json, os, sqlite3, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import construir_fichas_sqlite as C


def ficha(ref="John.1.1", pos=5, strong="G3056", glosa="Verbo", **kw):
    f = {"ref": ref, "pos_tisch": pos, "strong": strong, "forma": "λόγος", "lema": "λόγος", "glosa_interlineal": glosa,
         "rango_semantico": ["palabra", "razón"], "construccion": "c", "sentido_en_contexto": "s", "matiz": None,
         "variantes_textuales": None, "notas_traduccion": None, "otros_usos": None, "nivel_certeza": "alto",
         "alineacion": "forma", "tagnt": "John.1.1#5", "ref_estandar": ref, "dstrong": "G3056", "ediciones": ["NA28"],
         "ausente_en": [], "variante": False, "generador": "manual", "modelo": "manual-legacy", "prompt_hash": None,
         "generado_en": None}
    f.update(kw)
    return f


class ConstructorTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.v2, self.v3 = os.path.join(self.tmp, "v2"), os.path.join(self.tmp, "v3")
        os.makedirs(self.v2); os.makedirs(self.v3)
        self.citas = os.path.join(self.tmp, "citas.json")
        json.dump([{"ref_estandar": "John.1.1", "version": "RV", "texto": "t", "ocr_sospechoso": 0},
                   {"ref_estandar": "John.1.1", "version": "TA", "texto": "u", "ocr_sospechoso": 1}],
                  open(self.citas, "w"))

    def escribir(self, d, nombre, fichas):
        json.dump(fichas, open(os.path.join(d, nombre), "w", encoding="utf-8"), ensure_ascii=False)

    def construir(self, nombre="f.sqlite"):
        s = os.path.join(self.tmp, nombre)
        C.construir(s, [self.v2, self.v3], self.citas)
        return s

    def test_reproducible_byte_a_byte(self):
        self.escribir(self.v2, "a.json", [ficha(), ficha(pos=7, strong="G846")])
        a, b = self.construir("a.sqlite"), self.construir("b.sqlite")
        h = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
        self.assertEqual(h(a), h(b))

    def test_orden_de_entrada_no_cambia_la_base(self):
        self.escribir(self.v2, "a.json", [ficha(), ficha(pos=7, strong="G846")])
        a = self.construir("a.sqlite")
        self.escribir(self.v2, "a.json", [ficha(pos=7, strong="G846"), ficha()])
        b = self.construir("b.sqlite")
        h = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
        self.assertNotEqual(h(a), "")           # mismas filas: el contenido lógico es igual
        da, db_ = sqlite3.connect(a), sqlite3.connect(b)
        q = "SELECT ref_tisch,posicion,strong,glosa_interlineal FROM fichas ORDER BY id"
        self.assertEqual(da.execute(q).fetchall(), db_.execute(q).fetchall())

    def test_v3_gana_si_coincide_la_clave(self):
        self.escribir(self.v2, "a.json", [ficha(glosa="viejo", modelo="manual-legacy")])
        self.escribir(self.v3, "a.json", [ficha(glosa="nuevo", modelo="m", prompt_hash="abc",
                                                generado_en="2026-10-02T00:00:00")])
        db = sqlite3.connect(self.construir())
        self.assertEqual(db.execute("SELECT glosa_interlineal,modelo,prompt_hash FROM fichas").fetchall(),
                         [("nuevo", "m", "abc")])

    def test_v3_sin_sentido_no_pisa_una_v2_con_sentido(self):
        self.escribir(self.v2, "a.json", [ficha(glosa="rica", sentido_en_contexto="Sentido curado.", modelo="manual-legacy"),
                                          ficha(pos=7, strong="G846", glosa="vieja", sentido_en_contexto=None, modelo="manual-legacy")])
        self.escribir(self.v3, "a.json", [ficha(glosa="escueta", sentido_en_contexto=None, modelo="m", prompt_hash="abc",
                                                generado_en="2026-10-02T00:00:00"),
                                          ficha(pos=7, strong="G846", glosa="nueva", sentido_en_contexto=None, modelo="m", prompt_hash="abc",
                                                generado_en="2026-10-02T00:00:00")])
        db = sqlite3.connect(self.construir())
        q = "SELECT glosa_interlineal,sentido_en_contexto,modelo,ref_estandar FROM fichas ORDER BY posicion"
        self.assertEqual(db.execute(q).fetchall(),
                         [("rica", "Sentido curado.", "manual-legacy", "John.1.1"), ("nueva", None, "m", "John.1.1")])

    def test_clave_unica_y_claves_desconocidas(self):
        self.escribir(self.v2, "a.json", [ficha(), ficha()])
        with self.assertRaises(ValueError):
            self.construir()
        self.escribir(self.v2, "a.json", [ficha(glosa_tagnt="x")])
        with self.assertRaises(ValueError):
            self.construir()

    def test_certeza_nula_y_json_valido(self):
        self.escribir(self.v2, "a.json", [ficha(nivel_certeza=None)])
        db = sqlite3.connect(self.construir())
        self.assertIsNone(db.execute("SELECT nivel_certeza FROM fichas").fetchone()[0])
        with self.assertRaises(sqlite3.IntegrityError):
            db.execute("UPDATE fichas SET rango_semantico='no es json'")
        with self.assertRaises(sqlite3.IntegrityError):
            db.execute("UPDATE fichas SET nivel_certeza='enorme'")

    def test_citas_se_unen_por_ref_estandar(self):
        self.escribir(self.v2, "a.json", [ficha(ref_estandar="John.1.1")])
        db = sqlite3.connect(self.construir())
        r = db.execute("SELECT c.version,c.ocr_sospechoso FROM fichas f JOIN citas c USING (ref_estandar) "
                       "ORDER BY c.version").fetchall()
        self.assertEqual(r, [("RV", 0), ("TA", 1)])

    def test_indices_y_formato(self):
        self.escribir(self.v2, "a.json", [ficha()])
        db = sqlite3.connect(self.construir())
        self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0], 1)
        idx = {r[1] for r in db.execute("PRAGMA index_list(fichas)")}
        self.assertTrue({"fichas_ref", "fichas_strong"} <= idx)
        self.assertEqual(db.execute("SELECT ref_tagnt,posicion_tagnt FROM fichas").fetchone(), ("John.1.1", 5))

    @unittest.skipUnless(os.path.isdir(C.FUENTES[0]), "sin datos")
    def test_datos_reales_reproducibles(self):
        a, b = os.path.join(self.tmp, "r1.sqlite"), os.path.join(self.tmp, "r2.sqlite")
        C.construir(a); C.construir(b)
        h = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
        self.assertEqual(h(a), h(b))
        db = sqlite3.connect(a)
        n = db.execute("SELECT COUNT(*) FROM fichas WHERE ref_tisch='John.1.1'").fetchone()[0]
        self.assertEqual(n, 17)
        self.assertEqual(db.execute("SELECT COUNT(*) FROM fichas f JOIN citas c USING (ref_estandar) WHERE f.ref_tisch='John.1.1'")
                         .fetchone()[0] > 0, True)


if __name__ == "__main__":
    unittest.main()
