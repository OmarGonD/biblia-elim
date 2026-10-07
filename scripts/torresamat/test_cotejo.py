"""Cotejo de texto dañado y recuperación de fronteras de versículo."""
import cotejo

CAMBIOS = cotejo.lee_cotejo()


def test_todos_los_cotejos_y_segunda_pasada():
    assert len(CAMBIOS) == 17
    for key, record in CAMBIOS.items():
        assert cotejo.corrige(key, record["before"], CAMBIOS) == (record["after"], 1)
        assert cotejo.corrige(key, record["after"], CAMBIOS) == (record["after"], 0)


def test_recupera_solo_los_cuatro_versiculos_vacios():
    vacios = {key for key, r in CAMBIOS.items() if not r["before"]}
    assert vacios == {"Deuteronomy 20:4", "Psalms 119:3", "Proverbs 16:3", "Acts 27:38"}
    for key in vacios:
        assert cotejo.corrige(key, "", CAMBIOS)[0]
        try:
            cotejo.corrige(key, "Otro texto ya existente.", CAMBIOS)
        except ValueError:
            pass
        else:
            raise AssertionError("no puede sobrescribir un versículo ajeno al cotejo")


def test_hechos_separa_marineros_y_trigo():
    assert "áncoras" in CAMBIOS["Acts 27:30"]["after"]
    assert "trigo" not in CAMBIOS["Acts 27:30"]["after"]
    assert CAMBIOS["Acts 27:38"]["after"] == \
        "Estando ya satisfechos, aligeraban la nave, arrojando al mar el trigo."


def test_glosa_de_torres_y_ortografia():
    assert "ó de presentarte al juez" in CAMBIOS["Sirach 18:19"]["after"]
    assert "que voy á deciros" in CAMBIOS["Job 13:17"]["after"]
    assert "Racional" in CAMBIOS["Exodus 28:4"]["after"]
    assert "Ephod ó espaldar" in CAMBIOS["Exodus 28:4"]["after"]


def test_no_toca_otras_referencias():
    t = "llamar á convertir"
    assert cotejo.corrige("Mark 2:17", t, CAMBIOS) == (t, 0)


def test_protege_marcado():
    try:
        cotejo.corrige("Gen 1:1", '<note n="d"/>',
                       {"Gen 1:1": {"before": '<note n="d"/>', "after": '<note n="á"/>'}})
    except ValueError:
        pass
    else:
        raise AssertionError("no puede alterar atributos OSIS")


if __name__ == "__main__":
    for name, f in sorted(globals().copy().items()):
        if name.startswith("test_"):
            f()
            print("ok", name)
