import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools", "tagnt"))
import alinear, tagnt


def T(i, forma, strong):
    return {"pos": i, "forma": forma, "strong": strong, "morph": ""}


class P:
    def __init__(self, pos, griego, strong, alt=()):
        self.pos, self.griego, self.strong, self.alt_strongs = pos, griego, strong, list(alt)


class AlinearTest(unittest.TestCase):
    def test_extra_word_in_tisch_queda_sin_pareja(self):
        tw = [T(1, "καὶ", "G2532"), T(2, "ἐγένετο", "G1096"), T(3, "ὁ", "G3588"), T(4, "λόγος", "G3056")]
        pw = [P(1, "καὶ", "G2532"), P(2, "ὁ", "G3588"), P(3, "λόγος", "G3056")]
        self.assertEqual(alinear.nw(tw, pw), [(0, 0), (1, None), (2, 1), (3, 2)])

    def test_pronombre_con_otro_strong_empareja_por_familia(self):
        self.assertIsNotNone(alinear._puntaje(T(1, "ἡμῶν", "G2249"), P(1, "ἡμῶν", "G1473")))

    def test_forma_igual_distinto_strong_se_marca(self):
        t, p = T(1, "ἦν", "G1510"), P(1, "ἦν", "G2258")
        self.assertEqual(alinear.calidad(t, p), "forma_otro_strong")

    def test_ignora_acentos_y_puntuacion(self):
        self.assertEqual(alinear.norm("λόγος,"), alinear.norm("λογοσ"))
        self.assertEqual(alinear.norm("ἀλλ᾽"), alinear.norm("ἀλλ"))


if __name__ == "__main__":
    unittest.main()
