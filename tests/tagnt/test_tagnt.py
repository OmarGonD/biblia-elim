import os, sys, unicodedata, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools", "tagnt"))
import tagnt

HAY_DATOS = all(os.path.exists(os.path.join(tagnt.DIR_DATOS, a)) for a in tagnt.ARCHIVOS)

FILA = ("Jhn.1.1#12=NKO\tθεόν, (theon)\tGod,\tG2316=N-ASM-T\tθεός=God\t"
        "NA28+NA27+Tyn+SBL+WH+Treg+TR+Byz\t\t\tDios\tGod\t#12\tG2316_A\t\t\t\t\t")


class Linea(unittest.TestCase):
    def test_campos(self):
        p = tagnt.parsear_linea(FILA)
        self.assertEqual((p.ref, p.pos, p.tipo), ("John.1.1", 12, "NKO"))
        self.assertEqual((p.griego, p.translit), ("θεόν,", "theon"))
        self.assertEqual((p.dstrong, p.strong, p.gramatica), ("G2316", "G2316", "N-ASM-T"))
        self.assertEqual((p.lema, p.glosa, p.espanol), ("θεός", "God", "Dios"))
        self.assertEqual(p.ssi, "G2316_A")
        self.assertEqual(len(p.ediciones), 8)

    def test_strong_extendido_y_ceros(self):
        self.assertEqual(tagnt.strong_simple("G0976"), "G976")
        self.assertEqual(tagnt.strong_simple("G2424G"), "G2424")
        self.assertEqual(tagnt.strong_simple("H1732"), "H1732")
        self.assertEqual(tagnt.strong_simple(""), "")

    def test_no_es_fila(self):
        for l in ("# Mat.1.1\tΒίβλος", "#_Translation\tx", "Word & Type\tGreek", "", "Reference:\tx"):
            self.assertIsNone(tagnt.parsear_linea(l))

    def test_versificacion_alterna(self):
        l = "Mat.17.15[17.14]#01=NKO\tκαὶ (kai)\tand\tG2532=CONJ\tκαί=and\tNA28+NA27\t\t\tY\tand\t#01\tG2532_A"
        p = tagnt.parsear_linea(l)
        self.assertEqual((p.ref, p.ref_alt), ("Matt.17.15", "[17.14]"))

    def test_enlace(self):
        l = "Jhn.1.1#11=NKO\tτὸν (ton)\t<the>\tG3588=T-ASM\tὁ=the\tNA28\t\t\tel\tthe\t#11»12:G2316\tG3588_C"
        p = tagnt.parsear_linea(l)
        self.assertEqual((p.enlace_dir, p.enlace_pos, p.enlace_strong), ("»", 12, "G2316"))

    def test_otras_fuentes(self):
        l = "Luk.24.18#05=O\tἐξ (ex)\tof\tG1537=PREP\tἐκ=out\tNIV+KJV+Coptic\t\t\tde\tfrom\t#05\tG1537"
        p = tagnt.parsear_linea(l)
        self.assertEqual(p.ediciones, [])
        self.assertEqual(p.otras_fuentes, ["NIV", "KJV", "Coptic"])


@unittest.skipUnless(HAY_DATOS, "faltan los archivos del TAGNT (tools/tagnt/fetch_tagnt.sh)")
class Datos(unittest.TestCase):
    def test_jn_1_1(self):
        v = tagnt.versiculo("John.1.1")
        self.assertEqual(len(v), 17)
        self.assertEqual([p.pos for p in v], list(range(1, 18)))
        self.assertEqual("".join(p.griego for p in v).replace(",", "").replace(".", ""),
                         unicodedata.normalize("NFC", "Ἐνἀρχῇἦνὁλόγοςκαὶὁλόγοςἦνπρὸςτὸνθεόνκαὶθεὸςἦνὁλόγος"))
        self.assertEqual([p.strong for p in v if p.lema == "λόγος"], ["G3056"] * 3)
        # θεός x2: posición 12 (con artículo, ἀccusativo) y 14 (sin artículo, nominativo)
        d = {p.pos: p for p in v if p.strong == "G2316"}
        self.assertEqual(sorted(d), [12, 14])
        self.assertEqual(d[12].gramatica, "N-ASM-T")
        self.assertEqual(d[14].gramatica, "N-NSM-T")
        self.assertEqual(d[12].ssi, "G2316_A")
        self.assertEqual(d[14].ssi, "G2316_B")
        self.assertTrue(all(len(p.ediciones) == 8 for p in v))
        self.assertTrue(all(p.tipo == "NKO" for p in v))

    def test_jn_1_2(self):
        v = tagnt.versiculo("John.1.2")
        self.assertEqual([p.griego for p in v],
                         [unicodedata.normalize("NFC", w) for w in ["οὗτος", "ἦν", "ἐν", "ἀρχῇ", "πρὸς", "τὸν", "θεόν."]])
        pros = [p for p in v if p.strong == "G4314"][0]
        self.assertEqual((pros.gramatica, pros.pos), ("PREP", 5))   # la morfología no trae el caso regido

    def test_mc_16_9_final_largo(self):
        v = tagnt.versiculo("Mark.16.9")
        self.assertGreater(len(v), 10)
        self.assertTrue(all(p.tipo == "KO" for p in v[:8]))
        self.assertTrue(v[0].entre_corchetes)
        for p in v:
            self.assertFalse(p.en("SBL"))       # SBL no lleva Mc 16:9-20 en el TAGNT
            self.assertTrue(p.en("NA28") or p.en("TR"))
        self.assertTrue(v[0].en("Byz") and v[0].en("TR") and v[0].en("WH"))

    def test_mc_16_8_palabra_solo_en_tr(self):
        v = {p.pos: p for p in tagnt.versiculo("Mark.16.8")}
        self.assertEqual(v[3].griego, "ταχὺ")
        self.assertEqual(v[3].ediciones, ["TR"])
        self.assertIn("traditional", v[3].nota_variante)
        self.assertEqual(v[9].tipo, "N(k)O")
        self.assertFalse(v[9].en("TR"))

    def test_totales(self):
        n = sum(1 for _ in tagnt.leer())
        self.assertEqual(n, 142096)


if __name__ == "__main__":
    unittest.main()
