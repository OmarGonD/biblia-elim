import copy, json, os, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools", "tagnt"))
sys.path.insert(0, os.path.dirname(__file__))
try:
    import jsonschema  # noqa: F401
    import pipeline, comparar_manual_api, informe_costo
    from test_pipeline import respuesta_valida
    HAY = os.path.exists(pipeline.alinear.SALIDA)
except ImportError:
    HAY = False


class Lote:
    id = "msgbatch_prueba"
    processing_status = "ended"

    class request_counts:
        succeeded = 2

        @staticmethod
        def model_dump():
            return {"succeeded": 2}


class Item:
    def __init__(self, d):
        self.d = d

    def model_dump(self):
        return self.d


class ClienteFalso:
    def __init__(self, resultados=None):
        self.pedidos, self.res = None, resultados or []
        outer = self

        class Batches:
            @staticmethod
            def create(requests):
                outer.pedidos = requests
                return Lote()

            @staticmethod
            def retrieve(i):
                return Lote()

            @staticmethod
            def results(i):
                return [Item(r) for r in outer.res]

        class Messages:
            batches = Batches()

        self.messages = Messages()


def resultado(datos, ref, cards, usage=None):
    texto = json.dumps(cards, ensure_ascii=False)
    return {"custom_id": pipeline.custom_id(ref), "result": {"type": "succeeded", "message": {
        "content": [{"type": "text", "text": texto}],
        "usage": usage or {"input_tokens": 100, "output_tokens": 500, "cache_creation_input_tokens": 0,
                           "cache_read_input_tokens": 1200}}}}


@unittest.skipUnless(HAY, "faltan jsonschema o los datos")
class ApiTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.datos = pipeline.Datos()

    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def test_la_clave_solo_sale_del_entorno(self):
        viejo = os.environ.pop("ANTHROPIC_API_KEY", None)
        try:
            with self.assertRaises(RuntimeError) as c:
                pipeline.cliente_anthropic()
            self.assertIn("ANTHROPIC_API_KEY", str(c.exception))
        finally:
            if viejo is not None:
                os.environ["ANTHROPIC_API_KEY"] = viejo
        # ningún archivo del repositorio contiene una clave
        raiz = pipeline.RAIZ
        for ruta in ("tools/tagnt/pipeline.py", "tools/tagnt/informe_costo.py", "tools/tagnt/comparar_manual_api.py"):
            with open(os.path.join(raiz, ruta), encoding="utf-8") as f:
                self.assertNotIn("sk-ant-", f.read())

    def test_custom_id_valido_y_reversible(self):
        import re
        self.assertTrue(re.match(r"^[a-zA-Z0-9_-]{1,64}$", pipeline.custom_id("1Cor.13.4")))
        self.assertEqual(pipeline.ref_de_custom_id(pipeline.custom_id("1Cor.13.4")), "1Cor.13.4")

    def test_enviar_lote_con_cache_de_prompt(self):
        cli = ClienteFalso()
        i = pipeline.batch_enviar(self.datos, ["John.1.1", "John.1.2"], "claude-sonnet-5-5", cliente=cli)
        self.assertEqual(i, "msgbatch_prueba")
        self.assertEqual([r["custom_id"] for r in cli.pedidos], ["John_1_1", "John_1_2"])
        p = cli.pedidos[0]["params"]
        self.assertEqual(p["model"], "claude-sonnet-5-5")
        self.assertEqual(p["system"][0]["cache_control"], {"type": "ephemeral"})
        self.assertEqual(p["system"][0]["text"], pipeline.prompt_texto())
        self.assertEqual(pipeline.batch_estado("x", cli)["estado"], "ended")

    def test_no_se_pide_jn_8_12_53(self):
        with self.assertRaises(ValueError):
            pipeline.solicitudes_batch(self.datos, ["John.8.20"], "m")

    def test_traer_importar_y_medir_tokens(self):
        refs = ["John.1.1", "John.1.2"]
        res = [resultado(self.datos, r, respuesta_valida(pipeline.solicitud(self.datos, r))) for r in refs]
        res.append({"custom_id": "John_1_3", "result": {"type": "errored"}})
        cli = ClienteFalso(res)
        ruta = os.path.join(self.tmp, "res.jsonl")
        self.assertEqual(pipeline.batch_traer("x", ruta, cli), 3)
        cache = pipeline.Cache("m-api", raiz=self.tmp)
        uso, pend = os.path.join(self.tmp, "uso.jsonl"), os.path.join(self.tmp, "pend.json")
        r = pipeline.batch_import(self.datos, ruta, "m-api", cache, os.path.join(self.tmp, "e.jsonl"), uso, pend)
        self.assertEqual(r, {"ok": 2, "error": 1})
        self.assertIsNotNone(cache.get("John.1.1"))
        self.assertIn("John.1.3", json.load(open(pend)))                # quedó para un reintento
        medidas = informe_costo.leer_uso("m-api", set(refs), uso)
        self.assertEqual(medidas["John.1.1"]["cache_read_input_tokens"], 1200)

    def test_calculo_de_costo(self):
        uso = {"a": {"input_tokens": 100, "output_tokens": 400, "cache_creation_input_tokens": 1000, "cache_read_input_tokens": 0},
               "b": {"input_tokens": 200, "output_tokens": 600, "cache_creation_input_tokens": 0, "cache_read_input_tokens": 1000}}
        c = informe_costo.calcular(uso, palabras_corrida=10, n_calls_nt=100, palabras_nt=1000)
        self.assertEqual(c["prompt_sistema"], 1000)
        self.assertAlmostEqual(c["por_palabra"]["entrada_variable"], 30.0)    # (300 sin caché) / 10 palabras
        self.assertAlmostEqual(c["por_palabra"]["salida"], 100.0)
        self.assertEqual(c["nt"]["salida"], 100000)
        base = informe_costo.costo(c, 3, 15, 0.5, 0.1, 1.25, con_cache=False, batch=False)
        self.assertAlmostEqual(base, 30000 / 1e6 * 3 + 100000 / 1e6 * 15 + 100 * 1000 / 1e6 * 3)
        self.assertAlmostEqual(informe_costo.costo(c, 3, 15, 0.5, 0.1, 1.25, con_cache=False, batch=True), base / 2)

    def test_comparacion_manual_api(self):
        s = pipeline.solicitud(self.datos, "John.1.1")
        man = respuesta_valida(s)
        api = copy.deepcopy(man)
        api[9]["glosa_interlineal"] = "hacia"                          # πρὸς: la API difiere
        api[2]["nivel_certeza"] = "medio"
        pipeline.Cache("man", raiz=self.tmp).put("John.1.1", man)
        pipeline.Cache("api", raiz=self.tmp).put("John.1.1", api)
        salida = os.path.join(self.tmp, "cmp.md")
        rc = comparar_manual_api.main(["--refs", "John.1.1", "--manual-modelo", "man", "--api-modelo", "api",
                                       "--raiz-cache", self.tmp, "--salida", salida])
        self.assertEqual(rc, 0)
        txt = open(salida, encoding="utf-8").read()
        self.assertIn("Las 4 fichas de prueba", txt)
        self.assertIn("⟵ difiere", txt)
        self.assertIn("Mismo `nivel_certeza`", txt)


if __name__ == "__main__":
    unittest.main()
