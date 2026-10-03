import glob, json, os, sys, unittest
RAIZ = os.path.join(os.path.dirname(__file__), "..", "..")
sys.path.insert(0, os.path.join(RAIZ, "tools", "tagnt"))
import alinear, contexto, tagnt

V2 = os.path.join(RAIZ, "data", "fichas_v2")


class P:
    def __init__(self, griego, gram):
        self.griego, self.gramatica = griego, gram


class CasoRegidoTest(unittest.TestCase):
    def test_prep_con_articulo_y_nombre(self):
        v = [P("πρὸς", "PREP"), P("τὸν", "T-ASM"), P("θεόν", "N-ASM-T")]
        self.assertEqual(contexto.caso_regido(v, 0)[:2], ("A", False))

    def test_conjuncion_interpuesta_es_ambigua(self):
        v = [P("ἐν", "PREP"), P("καὶ", "CONJ"), P("τῷ", "T-DSM")]
        self.assertEqual(contexto.caso_regido(v, 0)[:2], (None, True))

    def test_articulo_y_nombre_discordantes_es_ambiguo(self):
        v = [P("διὰ", "PREP"), P("τοῦ", "T-GSN"), P("λόγον", "N-ASM")]
        self.assertTrue(contexto.caso_regido(v, 0)[1])

    def test_pospositiva_no_interrumpe(self):
        v = [P("ἐν", "PREP"), P("δὲ", "CONJ"), P("τῷ", "T-DSM"), P("οἴκῳ", "N-DSM")]
        self.assertEqual(contexto.caso_regido(v, 0)[:2], ("D", False))

    def test_infinitivo_articular_rige_el_articulo(self):
        v = [P("πρὸ", "PREP"), P("τοῦ", "T-GSN"), P("φωνῆσαι", "V-AAN"), P("σε", "P-2AS")]
        self.assertEqual(contexto.caso_regido(v, 0)[:2], ("G", False))

    def test_conjuncion_ante_verbo_no_es_ambigua(self):
        v = [P("ὅπου", "PREP"), P("ἦν", "V-IAI-3S")]
        self.assertEqual(contexto.caso_regido(v, 0)[:2], (None, False))
        v = [P("ἕως", "PREP"), P("ἄρτι", "ADV")]
        self.assertEqual(contexto.caso_regido(v, 0)[:2], (None, False))


@unittest.skipUnless(glob.glob(os.path.join(V2, "*.json")), "fichas_v2 no generadas")
class FichasV2Test(unittest.TestCase):
    def test_posicion_y_strong_coinciden_con_tisch(self):
        tisch = alinear.leer_tisch()
        n = 0
        for ruta in glob.glob(os.path.join(V2, "*.json")):
            for f in json.load(open(ruta, encoding="utf-8")):
                o, c, v = f["ref"].split(".")
                t = tisch[(o, int(c), int(v))][f["pos_tisch"] - 1]
                self.assertEqual(t["strong"], f["strong"], f["ref"])
                self.assertNotIn("traducciones_comparadas", f)
                self.assertEqual((f["generador"], f["modelo"]), ("manual", "manual-legacy"))
                n += 1
        self.assertGreaterEqual(n, 4166)

    def test_clave_unica(self):
        vistas = set()
        for ruta in glob.glob(os.path.join(V2, "*.json")):
            for f in json.load(open(ruta, encoding="utf-8")):
                k = (f["ref"], f["pos_tisch"], f["strong"])
                self.assertNotIn(k, vistas)
                vistas.add(k)


if __name__ == "__main__":
    unittest.main()
