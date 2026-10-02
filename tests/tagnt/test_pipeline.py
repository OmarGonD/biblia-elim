import copy, json, os, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools", "tagnt"))
try:
    import jsonschema  # noqa: F401
    import pipeline
    HAY = os.path.exists(pipeline.alinear.SALIDA)
except ImportError:
    HAY = False


def respuesta_valida(sol):
    """Respuesta sintética correcta para la solicitud (contenido de relleno: prueba la estructura)."""
    out = []
    for w in sol["palabras"]:
        basica = w["nivel"] == "basico"
        marcada = w.get("variante") or w.get("no_en_na28") or w.get("lectura_tagnt")
        out.append({"pos_tisch": w["pos"], "strong": w["strong"], "glosa_interlineal": "x",
                    "rango_semantico": ["a"], "construccion": ("caso %s" % w["caso_regido"][:5]) if w.get("caso_regido") else "c",
                    "sentido_en_contexto": None if basica else "s", "matiz": None,
                    "variantes_textuales": ("Tisch lee %s; las ediciones leen %s" % (w["forma"], w["lectura_tagnt"]["forma"])
                                            if w.get("lectura_tagnt") else "v") if marcada else None, "notas_traduccion": None, "otros_usos": None,
                    "nivel_certeza": "medio" if "caso_regido_ambiguo" in w else "alto"})
    return out


@unittest.skipUnless(HAY, "faltan jsonschema o los datos")
class PipelineTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.datos = pipeline.Datos()

    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.log = os.path.join(self.tmp, "err.jsonl")
        self.cache = pipeline.Cache("modelo-prueba", raiz=self.tmp)

    def test_solicitud_jn_1_1(self):
        s = pipeline.solicitud(self.datos, "John.1.1")
        self.assertEqual(len(s["palabras"]), 17)
        self.assertEqual([w["nivel"] for w in s["palabras"]].count("basico"), 6)
        self.assertEqual(s["palabras"][9]["caso_regido"], "acusativo")   # πρὸς τὸν θεόν

    def test_lectura_propia_de_tisch(self):
        w4 = pipeline.solicitud(self.datos, "John.1.4")["palabras"][3]       # ἐστιν; todas las ediciones leen ἦν
        self.assertEqual(w4["lectura_tagnt"]["forma"], "ἦν")
        s18 = pipeline.solicitud(self.datos, "John.1.18")                  # Tisch υἱός; NA28 θεός
        w = s18["palabras"][6]
        self.assertEqual((w["forma"], w["lectura_tagnt"]["forma"]), ("υἱὸς", "θεὸς"))
        self.assertNotIn("Tyn", w["lectura_tagnt"]["ediciones"])
        self.assertTrue(s18["palabras"][4]["no_en_na28"])                  # ὁ ante μονογενής
        self.assertIn("textos_pd", s18)

    def test_ortografia_no_es_variante(self):
        for r, pos in (("John.1.21", 6), ("John.1.39", 7)):               # Ἡλείας/Ἠλίας, ῥαββεί/ῥαββί
            self.assertNotIn("lectura_tagnt", pipeline.solicitud(self.datos, r)["palabras"][pos - 1])

    def test_respuesta_valida_y_errores(self):
        s = pipeline.solicitud(self.datos, "John.1.2")
        ok = respuesta_valida(s)
        self.assertEqual(pipeline.validar(ok, s), [])
        mal = copy.deepcopy(ok)
        mal[0]["traducciones_comparadas"] = []
        self.assertTrue(pipeline.validar(mal, s))                      # clave no permitida
        mal = copy.deepcopy(ok)
        mal[1]["glosa_interlineal"] = "RVR1960 dice"
        self.assertTrue(pipeline.validar(mal, s))                      # cita de versión
        self.assertTrue(pipeline.validar(ok[:-1], s))                  # falta una palabra
        basica = copy.deepcopy(ok)
        i = next(n for n, w in enumerate(s["palabras"]) if w["nivel"] == "basico")
        basica[i]["matiz"] = "no debería"
        self.assertTrue(pipeline.validar(basica, s))

    def test_reintento_y_cache(self):
        s = pipeline.solicitud(self.datos, "John.1.2")
        llamadas = []

        class Falso:
            modelo = "modelo-prueba"

            def generar(self, sol, error_previo=None):
                llamadas.append(error_previo)
                return [] if len(llamadas) == 1 else respuesta_valida(sol)

        self.assertEqual(pipeline.procesar(self.datos, "John.1.2", Falso(), self.cache, self.log), "ok")
        self.assertEqual(len(llamadas), 2)                             # un reintento con el error
        self.assertTrue(llamadas[1])
        self.assertEqual(len(open(self.log).read().splitlines()), 1)   # el primer intento quedó en el registro
        self.assertEqual(pipeline.procesar(self.datos, "John.1.2", Falso(), self.cache, self.log), "cache")
        self.assertEqual(len(llamadas), 2)                             # reanudable: no vuelve a llamar

    def test_fallo_tras_un_reintento(self):
        class Malo:
            modelo = "modelo-prueba"

            def generar(self, sol, error_previo=None):
                return []

        self.assertEqual(pipeline.procesar(self.datos, "John.1.2", Malo(), self.cache, self.log), "fallo")
        self.assertEqual(len(open(self.log).read().splitlines()), 2)
        self.assertIsNone(self.cache.get("John.1.2"))

    def test_manual_queda_pendiente(self):
        self.assertEqual(pipeline.procesar(self.datos, "John.1.2", pipeline.GeneradorManual(),
                                           pipeline.Cache(pipeline.MODELO_MANUAL, raiz=self.tmp), self.log), "pendiente")

    def test_cache_separada_por_prompt_y_modelo(self):
        a = pipeline.Cache("a", raiz=self.tmp)
        b = pipeline.Cache("b", raiz=self.tmp)
        self.assertNotEqual(a.dir, b.dir)
        self.assertIn(pipeline.prompt_hash(), a.dir)

    def test_api_cliente_falso_y_batch(self):
        s = pipeline.solicitud(self.datos, "John.1.2")

        class Bloque:
            type = "text"
            text = "```json\n" + json.dumps(respuesta_valida(s)) + "\n```"

        class Cli:
            class messages:
                @staticmethod
                def create(**kw):
                    assert kw["system"][0]["cache_control"] and kw["model"] == "m"
                    return type("R", (), {"content": [Bloque()]})()

        g = pipeline.GeneradorAPI("m", cliente=Cli())
        self.assertEqual(pipeline.procesar(self.datos, "John.1.2", g, pipeline.Cache("m", raiz=self.tmp), self.log), "ok")
        lote = os.path.join(self.tmp, "lote.jsonl")
        pipeline.batch_export(self.datos, ["John.1.2"], lote, "m")
        self.assertEqual(json.loads(open(lote).readline())["custom_id"], "John_1_2")
        res = os.path.join(self.tmp, "res.jsonl")
        with open(res, "w") as f:
            f.write(json.dumps({"custom_id": "John_1_2", "result": {"type": "succeeded",
                    "message": {"content": [{"type": "text", "text": Bloque.text}]}}}) + "\n")
            f.write(json.dumps({"custom_id": "John_1_3", "result": {"type": "errored"}}) + "\n")
        c2 = pipeline.Cache("m2", raiz=self.tmp)
        self.assertEqual(pipeline.batch_import(self.datos, res, "m2", c2, self.log), {"ok": 1, "error": 1})

    def test_ensamblar_con_versiones_pd(self):
        s = pipeline.solicitud(self.datos, "John.1.2")
        self.cache.put("John.1.2", respuesta_valida(s))
        ver = {("John", 1, 2): [{"version": "V", "texto": "t"}]}   # clave en numeración estándar
        r = pipeline.ensamblar(self.datos, ["John.1.2"], "modelo-prueba", ver, self.cache,
                               os.path.join(self.tmp, "v3"))
        self.assertEqual(r, {"John.01": 7})
        with open(os.path.join(self.tmp, "v3", "John.01.json")) as fh:
            f = json.load(fh)
        self.assertEqual(f[0]["traducciones_comparadas"], [{"version": "V", "ref": "John.1.2", "texto": "t"}])
        self.assertEqual(f[0]["ref_estandar"], "John.1.2")
        self.assertEqual(f[2]["caso_regido"], "dativo")
        self.assertNotIn("glosa_tagnt", f[0])

    def test_versiones_exactas_y_ocr(self):
        todo = pipeline.textos_versiones()
        # texto por la biblioteca SWORD, sin heurística; los versículos con restos de OCR llevan aviso
        self.assertEqual(todo[("John", 1, 2)][1]["texto"], "Él estaba en el principio en Dios f")
        self.assertIn("aviso", todo[("John", 1, 2)][1])
        self.assertNotIn("aviso", todo[("John", 1, 1)][1])
        self.assertNotIn("aviso", todo[("John", 1, 1)][0])
        self.assertEqual([x["version"] for x in todo[("John", 1, 1)]],
                         ["La Santa Biblia Reina-Valera (1909)", "La Sagrada Biblia (Torres Amat)"])

    def test_torres_amat_se_omite_en_versiculos_con_ocr(self):
        v = self.datos.versiones("John.1.2")                 # TorresAmat trae «f» suelta: solo RV 1909
        self.assertEqual([e["version"] for e in v], ["La Santa Biblia Reina-Valera (1909)"])
        v = self.datos.versiones("John.1.1")
        self.assertEqual(len(v), 2)

    def test_numeracion_estandar_de_jn_1_39_51(self):
        d = self.datos
        self.assertEqual(d.std_refs("John.1.39"), ["John.1.38"])
        self.assertEqual(d.std_refs("John.1.40"), ["John.1.39"])
        self.assertEqual(d.std_refs("John.1.51"), ["John.1.50", "John.1.51"])
        v = {e["ref"]: e["texto"] for e in d.versiones("John.1.40") if "Reina" in e["version"]}
        self.assertTrue(v["John.1.39"].startswith("Díceles: Venid y ved"))     # no el 1:40 de la RV 1909

    def test_torres_amat_se_mapea_de_vulgata_a_estandar(self):
        todo = pipeline.textos_versiones()
        ta = [e for e in todo[("Mark", 9, 50)] if "Torres" in e["version"]][0]["texto"]
        self.assertTrue(ta.startswith("La sal de suyo es buena"))             # KJV 9:50 = Vulg 9:49
        self.assertEqual(todo[("Rev", 13, 1)][1]["texto"][:20], "Y apostóse sobre la ")

    def test_jn_8_12_53_excluido(self):
        self.assertFalse(any(r.startswith("John.8.") and 12 <= int(r.split(".")[2]) <= 53
                             for r in self.datos.versiculos("John")))
        with self.assertRaises(ValueError):
            pipeline.solicitud(self.datos, "John.8.53")

    def test_lectura_de_tisch_debe_decirlo(self):
        s = pipeline.solicitud(self.datos, "John.1.4")
        self.assertTrue(s["palabras"][3]["lectura_tagnt"]["propia_de_tisch"])
        ok = respuesta_valida(s)
        ok[3]["variantes_textuales"] = "Tisch lee ἐστιν; todas las ediciones leen ἦν."
        self.assertEqual(pipeline.validar(ok, s), [])
        ok[3]["variantes_textuales"] = "Hay otra lectura."
        self.assertTrue(pipeline.validar(ok, s))


if __name__ == "__main__":
    unittest.main()
