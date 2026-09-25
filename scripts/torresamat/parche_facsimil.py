"""
Corrige versos sueltos del módulo TorresAmat ya compilado, cotejados a ojo
contra el facsímil de 1882.

Los datos de construcción (djvu.xml, texto.json) no viajan con el repo, así
que no se puede rehacer el módulo desde el principio. Este parche parte del
módulo instalado: lo exporta con mod2imp, sustituye solo las entradas de
CORRECCIONES -- y solo si el texto viejo coincide exactamente --, lo
reimporta con imp2vs y comprueba que la ida y vuelta no toca nada más.

Además de erratas (CORRECCIONES), marca los títulos de salmo que el impreso
numera como versículo (TITULOS; ver titulos.py) sin mover ningún número.

Cada entrada lleva el tomo y la hoja del ítem de Internet Archive donde se
ha leído el impreso:

    https://archive.org/details/la-sagrada-biblia-vulgata-tomo-iiv_202111

Uso:

    python3 parche_facsimil.py [--origen RAIZ] [--salida DIR]

RAIZ es un árbol SWORD (mods.d/ + modules/) del que se parte: por defecto el
instalado (~/.sword); para regenerar desde una fuente conocida, el modulos/
de git sin parchear (commit 9c036c87). Solo se lee, nunca se escribe. Deja
en DIR (por defecto salida/parche) un árbol SWORD completo con el módulo
corregido y el .conf del origen; aplicado otra vez sobre DIR no cambia nada.
Copiarlo a modulos/ y a ~/.sword es un paso aparte.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

import entidades
from cabeceras_pegadas import (AÑADIDOS, CABECERAS, NOTAS, REPETIDOS,
                               TRASLADADOS)
from columnas_fundidas import COLUMNAS
from fusiones_vacio import FUSIONES
from titulos import marca_titulo

DIR = os.path.dirname(os.path.abspath(__file__))
MODULO = "TorresAmat"
V11N = "Vulg"

# ref: (texto actual en el módulo, texto del impreso, dónde se ha leído)
CORRECCIONES = {
    # SALMO III. El «tú» de v. 4 salió como «44» y el v. 3 quedó pegado
    # delante del v. 5, dejando el 3 vacío.
    "Psalms 3:2": (
        "¡Ah Señor!;Cómo es que se han aumentado tanto mis perseguidores? "
        "Son muchísimos los que se han rebelado contra mí.",
        "¡Ah Señor! ¿Cómo es que se han aumentado tanto mis perseguidores? "
        "Son muchísimos los que se han rebelado contra mí.",
        "tomo III, hoja 11",
    ),
    "Psalms 3:3": (
        "",
        "Muchos dicen de mí: Ya no tiene que esperar de su Dios salvacion "
        "ó amparo.",
        "tomo III, hoja 11",
    ),
    "Psalms 3:4": (
        "Pero tú, oh Senor, 44 eres mi protector, mi gloria, y el que me "
        "haces levantar cabeza.",
        "Pero tú, oh Señor, tú eres mi protector, mi gloria, y el que me "
        "haces levantar cabeza.",
        "tomo III, hoja 11",
    ),
    "Psalms 3:5": (
        "Muchos dicen de mí: Ya no tiene que esperar de su Dios salvacion "
        "ó amparo. A veces clamé al Señor, y él me oyó benigno desde su "
        "santo monte",
        "A veces clamé al Señor, y él me oyó benigno desde su santo monte.",
        "tomo III, hoja 11",
    ),
    # SAN MATHEO, CAPITULO XII. El «$» es la llamada de nota 3 («Véase
    # Pan»), no texto; «ú solos» y «eon» son erratas de OCR, y el v. 11
    # perdió el renglón final.
    "Matthew 12:4": (
        "¿Cómo entró en la Casa de Dios, y comió los panes de la proposicion "
        "$, que no era lícito comer ni á él ni á los suyos, sino ú solos los "
        "sacerdotes?",
        "¿Cómo entró en la Casa de Dios, y comió los panes de la proposicion, "
        "que no era lícito comer ni á él ni á los suyos, sino á solos los "
        "sacerdotes?",
        "tomo IV, hoja 22",
    ),
    "Matthew 12:5": (
        "¿O no habeis leido en la Ley, cómo los sacerdotes en el templo "
        "trabajan en el sábado, y eon todo eso no pecan?",
        "¿O no habeis leido en la Ley, cómo los sacerdotes en el templo "
        "trabajan en el sábado, y con todo eso no pecan?",
        "tomo IV, hoja 22",
    ),
    # La «i» final no está impresa: el impreso acaba «el templo.» y el
    # djvu.xml pone en el blanco del renglón una caja de 3×8 px, confianza
    # 0 (x 1800-1803, y 5996-6004): una mota del papel.
    "Matthew 12:6": (
        "Pues yo os digo, que aquí está uno que es mayor que el templo. i",
        "Pues yo os digo, que aquí está uno que es mayor que el templo.",
        "tomo IV, hoja 22",
    ),
    # El impreso compone «hallaba un» casi sin espacio y el OCR (también
    # el djvu.xml del Archive) lo funde en «hallabaun».
    "Matthew 12:10": (
        "Donde se hallabaun hombre que tenia seca una mano; y preguntaron á "
        "Jesus, para hallar motivo de acusarle: ¿Si era lícito curar en dia "
        "de sábado?",
        "Donde se hallaba un hombre que tenia seca una mano; y preguntaron á "
        "Jesus, para hallar motivo de acusarle: ¿Si era lícito curar en dia "
        "de sábado?",
        "tomo IV, hoja 22",
    ),
    # Destinos de TRASLADOS (ver abajo): el cuerpo que el OCR pegó al
    # título del v. 1 pasa, sin tocar, al versículo vacío al que pertenece.
    "Psalms 84:2": (
        "",
        "Oh Señor, tá has derramado la bendicion sobre tu tierra: tú has "
        "libertado del cautiverio á Jacob.",
        "tomo III, hoja 46",
    ),
    "Psalms 13:2": (
        "",
        "El Señor echó desde el cielo una mirada sobre los hijos de los "
        "hombres, para ver si habia uno que tuviese juicio, ó que buscase"
        " á Dios.",
        "tomo III, hoja 14",
    ),
    "Psalms 9:22": (
        "",
        "¿Y por qué, oh Señor, te has retirado á lo lejos; y me has "
        "desamparado en el tiempo mas crítico, en la tribulaclon?",
        "tomo III, hoja 13",
    ),
    "Matthew 12:11": (
        "Mas él les dijo: ¿Qué hombre habrá entre vosotros, que tenga una "
        "oveja, y si esta cae en una fosa en dia de",
        "Mas él les dijo: ¿Qué hombre habrá entre vosotros, que tenga una "
        "oveja, y si esta cae en una fosa en dia de sábado, no la levante y "
        "saque fuera?",
        "tomo IV, hoja 22",
    ),
    # SALMO IV. El número del v. 3, arriba de la columna derecha, salió
    # «5.» y el versículo quedó vacío, pegado delante del v. 5 (como el
    # 3 y el 5 del Salmo III). Instalado con TORRES-PSALM-TITLES-101
    # (rama fix/torresamat-psalm-titles, ee2b0938) pero ausente aquí: sin
    # estas dos entradas el módulo instalado no se podía regenerar.
    "Psalms 4:3": (
        "",
        "Oh hijos de los hombres, ¿hasta cuándo sereis de estúpido corazon? "
        "¿por qué amais la vanidad y vais en pos de la mentira?",
        "tomo III, hoja 11",
    ),
    # El v. 5 conserva solo lo suyo; sus erratas quedan como estaban.
    # Huecos cuyo texto está en el facsímil de 1882 y no en el módulo.
    # Sin esto la app los suplía con Reina-Valera.
    "Psalms 31:2": (
        "",
        "Dichoso el hombre á quien el Señor no arguye de pecado; y cuya "
        "alma se halla exenta de dolo.",
        "tomo III, hoja 20",
    ),
    "Psalms 34:2": (
        "",
        "Ármate y abraza el escudo, y sal á defenderme.",
        "tomo III, hoja 21",
    ),
    "Psalms 4:5": (
        "Oh hijos de los hombres, ¿hasta cuándo sereis de estúpido corazon? "
        "¿por qué amais la vanidad y vais en pos de la mentira? Enojaos, y "
        "no querais pecar mas; compungíos en el retiro de vuestros lechos "
        "de las cosas que andais meditando en vuestros corazones",
        "Enojaos, y no querais pecar mas; compungíos en el retiro de "
        "vuestros lechos de las cosas que andais meditando en vuestros "
        "corazones",
        "tomo III, hoja 11",
    ),
    # Encabezado «SALMO …» y el argumento en cursiva del salmo siguiente,
    # pegados por el OCR al último versículo. No son inscripción: se quitan
    # y no se marcan. Hoja 11, Sal 4 acaba «10. … mi esperanza.»; hoja 30,
    # Sal 52 acaba «7. … gozo Israél.»; hoja 65, Sal 130 acaba «3. … jamás.».
    "Psalms 4:10": (
        "Porque tú, oh Señor, solo tú has asegurado mi esperanza +, SALMO "
        "Y Fervorosa oracion que hace David ú Dios; en la cual dice cuánto "
        "aborrece el Señor ú los malos, y cuánto ama y favorece a los "
        "buenos. <chapter eID=\"gen14339\" osisID=\"Ps.4\"/>",
        "Porque tú, oh Señor, solo tú has asegurado mi esperanza +, "
        "<chapter eID=\"gen14339\" osisID=\"Ps.4\"/>",
        "tomo III, hoja 11",
    ),
    "Psalms 52:7": (
        "¡Oh! ¿Quién enviará de Sion al Salvador de Israél?Cuando Dios "
        "pondrá fin al cautiverio de su pueblo, se regocijará Jacob, y "
        "saltará de gozo Israél. SALMO LIM Datid implora el auxilio de Dios "
        "contra sus enemigos; y promete, vencldos estos, conter las "
        "alubanzas de su Libertador <chapter eID=\"gen15120\" "
        "osisID=\"Ps.52\"/>",
        "¡Oh! ¿Quién enviará de Sion al Salvador de Israél?Cuando Dios "
        "pondrá fin al cautiverio de su pueblo, se regocijará Jacob, y "
        "saltará de gozo Israél. <chapter eID=\"gen15120\" "
        "osisID=\"Ps.52\"/>",
        "tomo III, hoja 30",
    ),
    "Psalms 130:3": (
        "Espere Israél en el Señor, desde ahora y por siempre jamás. SALMO "
        "CONXNXXI Ruega el pueblo á Dios que restaure su remo por medio del "
        "Mestas, Cántico gradual. Acuérdate de David, oh Señor, y de toda "
        "su gran mansedumbre <chapter eID=\"gen16579\" osisID=\"Ps.130\"/>",
        "Espere Israél en el Señor, desde ahora y por siempre jamás. "
        "<chapter eID=\"gen16579\" osisID=\"Ps.130\"/>",
        "tomo III, hoja 65",
    ),
}

# Versículos que el impreso numera y que son título del salmo (la Vulgata
# cuenta la inscripción como versículo). No se renumera nada: el texto se
# queda en su versículo y se marca como título (titulos.py).
#
# ref: (texto actual en el módulo, título impreso, cuerpo que comparte el
#       versículo o "", dónde se ha leído)
#
# El título es el del módulo cuando ya estaba; solo se toma del impreso lo
# que faltaba. Las erratas que traen (Sal 50:2) quedan para su propia
# corrección.
TITULOS = {
    "Psalms 3:1": (
        "Salmo de David cuando temeroso iba huyendo de su hijo Absalom",
        "Salmo de David cuando temeroso iba huyendo de su hijo Absalom",
        "",
        "tomo III, hoja 11",
    ),
    "Psalms 4:1": (
        "Para el fin: Salmo y Cántico de David.",
        "Para el fin: Salmo y Cántico de David.",
        "",
        "tomo III, hoja 11",
    ),
    # SALMO L. Título en dos versículos; el OCR perdió el primero.
    "Psalms 50:1": (
        "",
        "Para el fin: Salmo de David;",
        "",
        "tomo III, hoja 29",
    ),
    "Psalms 50:2": (
        "Cuando despues que pecó con Bethsabée, y vino: á él el Profeta "
        "Nathán 4",
        "Cuando despues que pecó con Bethsabée, y vino: á él el Profeta "
        "Nathán 4",
        "",
        "tomo III, hoja 29",
    ),
    # SALMO LI. Igual que el L.
    "Psalms 51:1": (
        "",
        "Para el fin: Salmo de inteligencia de David,",
        "",
        "tomo III, hoja 30",
    ),
    "Psalms 51:2": (
        "Cuando Doeg, Iduméo, fué á dar aviso á Saul, diciéndole que David "
        "habia estado en casa de Achimelech",
        "Cuando Doeg, Iduméo, fué á dar aviso á Saul, diciéndole que David "
        "habia estado en casa de Achimelech",
        "",
        "tomo III, hoja 30",
    ),
    # SALMO LII. El impreso pone «Para el fin: 1. Por Maeleth. Salmo de
    # inteligencia de David. Dijo el insensato…»: título y primer renglón
    # comparten el versículo 1. «Para el fin:» va delante del número y el
    # OCR lo perdió.
    "Psalms 52:1": (
        "Por Maeleth. Salmo de inteligencia de David. Dijo el insensato en "
        "su corazon: No hay Dios.",
        "Para el fin: Por Maeleth. Salmo de inteligencia de David.",
        "Dijo el insensato en su corazon: No hay Dios.",
        "tomo III, hoja 30",
    ),
    # Segundo lote (TORRES-PSALM-TITLES-101): versículos que el impreso da
    # solo como inscripción -- «1. Para el fin: …» y el cuerpo desde el 2 --,
    # cotejados uno a uno en su hoja. Dónde acaba la inscripción lo marca la
    # Vulgata Clementina con su cierre de párrafo (estructura, no texto); el
    # texto es el del módulo, sin tocar: sus erratas de OCR («Balmo de
    # Dayid», «Para el tin», «$») quedan para su propia corrección, y «Salvo
    # de David» (Sal 18) está así en el impreso. Sal 59 lleva la inscripción
    # en los versículos 1 y 2, como el 50 y el 51.
    "Psalms 5:1": (
        "Para el fin: por aquella que consigue la herencia: Salmo de David.",
        "Para el fin: por aquella que consigue la herencia: Salmo de David.",
        "",
        "tomo III, hoja 11",
    ),
    "Psalms 6:1": (
        "Para el fin: Cántico y Salmo de David para la octava.",
        "Para el fin: Cántico y Salmo de David para la octava.",
        "",
        "tomo III, hoja 11",
    ),
    "Psalms 18:1": (
        "Para el fin: Salvo de David",
        "Para el fin: Salvo de David",
        "",
        "tomo III, hoja 16",
    ),
    "Psalms 21:1": (
        "Para el fin:por el auxilio de la mañana, Salmo de David.",
        "Para el fin:por el auxilio de la mañana, Salmo de David.",
        "",
        "tomo III, hoja 17",
    ),
    "Psalms 29:1": (
        "Balmo de Dayid, cantado en la dedicacion de la casa de David.",
        "Balmo de Dayid, cantado en la dedicacion de la casa de David.",
        "",
        "tomo III, hoja 19",
    ),
    "Psalms 35:1": (
        "Para el tin: Salmo del mismo David siervo del Señor.",
        "Para el tin: Salmo del mismo David siervo del Señor.",
        "",
        "tomo III, hoja 24",
    ),
    "Psalms 39:1": (
        "Para el fin: Salmo del mismo David.",
        "Para el fin: Salmo del mismo David.",
        "",
        "tomo III, hoja 25",
    ),
    "Psalms 43:1": (
        "Para el fin á los hijos de Coré: Salmo de inteligencia.",
        "Para el fin á los hijos de Coré: Salmo de inteligencia.",
        "",
        "tomo III, hoja 26",
    ),
    "Psalms 59:1": (
        "Para el fin: Por aquellos que serán mudados. Inscripclon para una "
        "columna. Al mismo David para instruccion",
        "Para el fin: Por aquellos que serán mudados. Inscripclon para una "
        "columna. Al mismo David para instruccion",
        "",
        "tomo III, hoja 34",
    ),
    "Psalms 59:2": (
        "Cuando quemó la Mesopotamia de Syria y á Soba; y vuelto Joab, venció "
        "la Iduméa, derrotando doce $ mil hombres en el valle de las Salinas.",
        "Cuando quemó la Mesopotamia de Syria y á Soba; y vuelto Joab, venció "
        "la Iduméa, derrotando doce $ mil hombres en el valle de las Salinas.",
        "",
        "tomo III, hoja 34",
    ),
    "Psalms 88:1": (
        "Instruccion de Ethan Ezrahita",
        "Instruccion de Ethan Ezrahita",
        "",
        "tomo III, hoja 47",
    ),
    "Psalms 99:1": (
        "Salmo de la alabanza.",
        "Salmo de la alabanza.",
        "",
        "tomo III, hoja 51",
    ),
    # Tercer lote (TORRES-PSALM-TITLES-101), mismo criterio que el segundo:
    # el impreso da el «1.» solo como inscripción y el cuerpo desde el «2.»,
    # cotejado en cada hoja. Texto del módulo sin tocar: sus erratas de OCR
    # («Dayid», «éxtasi 0 exceso de Pen.», «Achimelech 3», «alabamea»,
    # «mistea) rios») quedan para su propia corrección.
    "Psalms 7:1": (
        "Salmo de David, cantado por él al Señor con motivo de las "
        "palabras de Chusi, hijo de Jemini",
        "Salmo de David, cantado por él al Señor con motivo de las "
        "palabras de Chusi, hijo de Jemini",
        "",
        "tomo III, hoja 12",
    ),
    "Psalms 10:1": (
        "Para el fin: Salmo de David.",
        "Para el fin: Salmo de David.",
        "",
        "tomo III, hoja 13",
    ),
    "Psalms 11:1": (
        "Para el fin: para la octava: Salmo de Dayid.",
        "Para el fin: para la octava: Salmo de Dayid.",
        "",
        "tomo III, hoja 13",
    ),
    "Psalms 17:1": (
        "Para el fin: Salmo de David, siervo del Señor, á cuya eloria "
        "dirigió las palabras de este cántico, en el dia en que le libró "
        "el Señor de las manos de todos sus enemigos, como tambien del "
        "poder de Saul, con cuyo motivo dijo",
        "Para el fin: Salmo de David, siervo del Señor, á cuya eloria "
        "dirigió las palabras de este cántico, en el dia en que le libró "
        "el Señor de las manos de todos sus enemigos, como tambien del "
        "poder de Saul, con cuyo motivo dijo",
        "",
        "tomo III, hoja 15",
    ),
    "Psalms 19:1": (
        "Para el fin: Salmo de David.",
        "Para el fin: Salmo de David.",
        "",
        "tomo III, hoja 16",
    ),
    "Psalms 20:1": (
        "Para el fin: Salmo de David.",
        "Para el fin: Salmo de David.",
        "",
        "tomo III, hoja 16",
    ),
    "Psalms 30:1": (
        "Para el fin: Salmo de David, por un éxtasi 0 exceso de Pen.",
        "Para el fin: Salmo de David, por un éxtasi 0 exceso de Pen.",
        "",
        "tomo III, hoja 19",
    ),
    "Psalms 33:1": (
        "Salmo de David, cuando se desfiguró delante del rey Achimelech "
        "3, el cual le echó de sí; con lo que David se escapó.",
        "Salmo de David, cuando se desfiguró delante del rey Achimelech "
        "3, el cual le echó de sí; con lo que David se escapó.",
        "",
        "tomo III, hoja 21",
    ),
    "Psalms 37:1": (
        "Salmo de David para recuerdo; en sábado.",
        "Salmo de David para recuerdo; en sábado.",
        "",
        "tomo III, hoja 25",
    ),
    "Psalms 40:1": (
        "Para el fin: Salmo del mismo David.",
        "Para el fin: Salmo del mismo David.",
        "",
        "tomo III, hoja 26",
    ),
    "Psalms 44:1": (
        "Para el fin: para aquellos que han de ser mudados ó trocados: Á "
        "los hijos de Coré, Salmo de inteligencia: Cántico en alabamea "
        "del amado",
        "Para el fin: para aquellos que han de ser mudados ó trocados: Á "
        "los hijos de Coré, Salmo de inteligencia: Cántico en alabamea "
        "del amado",
        "",
        "tomo III, hoja 27",
    ),
    "Psalms 45:1": (
        "Para el fin á los hijos de Coré: Salmo para los mistea) rios.",
        "Para el fin á los hijos de Coré: Salmo para los mistea) rios.",
        "",
        "tomo III, hoja 27",
    ),
    "Psalms 46:1": (
        "Para el fin: á los hijos de Coré, Salmo.",
        "Para el fin: á los hijos de Coré, Salmo.",
        "",
        "tomo III, hoja 28",
    ),
    "Psalms 47:1": (
        "Salmo de cántico: á los hijos de Coré: para el segundo dia de la"
        " semana",
        "Salmo de cántico: á los hijos de Coré: para el segundo dia de la"
        " semana",
        "",
        "tomo III, hoja 28",
    ),
    "Psalms 48:1": (
        "Para el fin: á los hijos de Coré, Salmo.",
        "Para el fin: á los hijos de Coré, Salmo.",
        "",
        "tomo III, hoja 28",
    ),
    # Cuarto lote (TORRES-PSALM-TITLES-101), mismo criterio. Sal 53 lleva
    # la inscripción en los versículos 1 y 2 (el cuerpo empieza en «3.
    # Sálvame»), como el 50, el 51 y el 59; el 60 solo en el 1 («2.
    # Escucha…»). Texto del módulo sin tocar.
    "Psalms 53:1": (
        "Para el fin: sobre los Cánticos. Salmo de inteligencia de David",
        "Para el fin: sobre los Cánticos. Salmo de inteligencia de David",
        "",
        "tomo III, hoja 30",
    ),
    "Psalms 53:2": (
        "Cuando fueron los Ziphéos á decir á Saul: ¿No sabes que David "
        "está escondido entre nosotros?",
        "Cuando fueron los Ziphéos á decir á Saul: ¿No sabes que David "
        "está escondido entre nosotros?",
        "",
        "tomo III, hoja 30",
    ),
    "Psalms 54:1": (
        "Para el fin: sobre los Cánticos. Salmo de inteligencia de David.",
        "Para el fin: sobre los Cánticos. Salmo de inteligencia de David.",
        "",
        "tomo III, hoja 30",
    ),
    "Psalms 60:1": (
        "Para el fin: sobre los Cánticos de David.",
        "Para el fin: sobre los Cánticos de David.",
        "",
        "tomo III, hoja 34",
    ),
    # Quinto lote (TORRES-PSALM-TITLES-101), mismo criterio: «1.» solo
    # inscripción, completa en el módulo; cuerpo desde el «2.». Texto sin
    # tocar («Dayid», «Senor», «Asaph» sin punto quedan como estaban).
    "Psalms 66:1": (
        "Para el fin, sobre los himnos: Salmo y Cántico de David",
        "Para el fin, sobre los himnos: Salmo y Cántico de David",
        "",
        "tomo III, hoja 36",
    ),
    "Psalms 67:1": (
        "Para el fin: Salmo y Cántico del mismo David.",
        "Para el fin: Salmo y Cántico del mismo David.",
        "",
        "tomo III, hoja 36",
    ),
    "Psalms 68:1": (
        "Para el fin: por los que han de ser mudados. Salmo de Dayid.",
        "Para el fin: por los que han de ser mudados. Salmo de Dayid.",
        "",
        "tomo III, hoja 37",
    ),
    "Psalms 69:1": (
        "Para el fin: Salmo de David, en memoria de haberle el Senor "
        "salvado.",
        "Para el fin: Salmo de David, en memoria de haberle el Senor "
        "salvado.",
        "",
        "tomo III, hoja 38",
    ),
    "Psalms 74:1": (
        "Para el fin: No nos destruyas. Salmo y Cántico de Asaph",
        "Para el fin: No nos destruyas. Salmo y Cántico de Asaph",
        "",
        "tomo III, hoja 40",
    ),
    "Psalms 75:1": (
        "Para el fin: para alabar. Salmo de Asaph. Cántico sobre los "
        "Assyrios.",
        "Para el fin: para alabar. Salmo de Asaph. Cántico sobre los "
        "Assyrios.",
        "",
        "tomo III, hoja 40",
    ),
    "Psalms 76:1": (
        "Para el fin: Para Idithun: Salmo de Asaph.",
        "Para el fin: Para Idithun: Salmo de Asaph.",
        "",
        "tomo III, hoja 40",
    ),
    # Sexto lote (TORRES-PSALM-TITLES-101), mismo criterio. Texto sin
    # tocar («d vendimia», «intelivencia», «Asaph. Salmo» sin punto).
    "Psalms 79:1": (
        "Para el fin: Para aquellos que han de ser mudados. Testimonio de"
        " Asaph. Salmo",
        "Para el fin: Para aquellos que han de ser mudados. Testimonio de"
        " Asaph. Salmo",
        "",
        "tomo III, hoja 44",
    ),
    "Psalms 82:1": (
        "Cántico y Salmo de Asaph.",
        "Cántico y Salmo de Asaph.",
        "",
        "tomo III, hoja 45",
    ),
    "Psalms 83:1": (
        "Para el fin, Para los lagares, d vendimia. Salmo para los hijos "
        "de Coré.",
        "Para el fin, Para los lagares, d vendimia. Salmo para los hijos "
        "de Coré.",
        "",
        "tomo III, hoja 46",
    ),
    "Psalms 101:1": (
        "Oracion de un miserable, que hallándose atribulado, derrama en "
        "la presencia del Señor sus plegarias.",
        "Oracion de un miserable, que hallándose atribulado, derrama en "
        "la presencia del Señor sus plegarias.",
        "",
        "tomo III, hoja 51",
    ),
    "Psalms 141:1": (
        "Salmo de intelivencia de David: su oracion cuando estaba en la "
        "cueva.",
        "Salmo de intelivencia de David: su oracion cuando estaba en la "
        "cueva.",
        "",
        "tomo III, hoja 68",
    ),
    # Recuperación (TORRES-PSALM-TITLES-101), como el Sal 52:1: el impreso
    # pone «Para el fin:» DELANTE del «1.» («Para el fin: 1. Salmo de
    # instruccion…») y el OCR lo perdió; el resto de la inscripción está
    # entero en el v. 1 del módulo y el cuerpo empieza en el «2.». Solo se
    # antepone «Para el fin: », leído en la hoja (y en el djvu.xml); el texto
    # que ya estaba no se toca (erratas «d Philisthéos», «Geth 1» -- la
    # llamada de nota 1 --, «ú tu siervo» quedan para su corrección).
    "Psalms 41:1": (
        "Salmo de instruccion, á los hijos de Coré.",
        "Para el fin: Salmo de instruccion, á los hijos de Coré.",
        "",
        "tomo III, hoja 26",
    ),
    "Psalms 55:1": (
        "Para la gente que estaba lejos del Santuario: Inscripcion para "
        "ponerse sobre una columna por David, cuando los extranjeros d "
        "Philisthéos le detuvieron en Geth 1",
        "Para el fin: Para la gente que estaba lejos del Santuario: "
        "Inscripcion para ponerse sobre una columna por David, cuando los"
        " extranjeros d Philisthéos le detuvieron en Geth 1",
        "",
        "tomo III, hoja 31",
    ),
    "Psalms 56:1": (
        "No destruyas ú tu siervo. Salmo de David para inscribirse en una"
        " columna, cuando huyendo de Saul, se retiró en una cueva",
        "Para el fin: No destruyas ú tu siervo. Salmo de David para "
        "inscribirse en una columna, cuando huyendo de Saul, se retiró en"
        " una cueva",
        "",
        "tomo III, hoja 31",
    ),
    "Psalms 57:1": (
        "No destruyas ú tu siervo. Salmo de David para inscribirse en una"
        " columna",
        "Para el fin: No destruyas ú tu siervo. Salmo de David para "
        "inscribirse en una columna",
        "",
        "tomo III, hoja 31",
    ),
    "Psalms 58:1": (
        "No destruyas ú tu siervo. Salmo de David para inscribirse en una"
        " columna: cuando Saul envió una guardia á su casa, con el fin de"
        " quitarle la vida",
        "Para el fin: No destruyas ú tu siervo. Salmo de David para "
        "inscribirse en una columna: cuando Saul envió una guardia á su "
        "casa, con el fin de quitarle la vida",
        "",
        "tomo III, hoja 31",
    ),
    # Recuperación, segundo lote: mismo patrón; el prefijo impreso delante
    # del «1.» varía («Salmo de David.», «Para el fin: Salmo de David.»,
    # «Cántico y Salmo.» y «Salmo y Cántico.» en renglón propio encima del
    # número, Sal 87 y 91). Sal 61:2-3 (cuerpo fundido, 61:3 vacío) no se
    # toca aquí.
    "Psalms 61:1": (
        "Salmo de David para Idithun.",
        "Para el fin: Salmo de David para Idithun.",
        "",
        "tomo III, hoja 34",
    ),
    "Psalms 62:1": (
        "Estando en el desierto de Iduméa.",
        "Salmo de David. Estando en el desierto de Iduméa.",
        "",
        "tomo III, hoja 35",
    ),
    "Psalms 63:1": (
        "Salmo de David.",
        "Para el fin: Salmo de David.",
        "",
        "tomo III, hoja 35",
    ),
    "Psalms 64:1": (
        "Cántico de Jeremías y de Ezechiel para el pueblo trasportado al "
        "cautiverio, cuando empezaba á salir de él.",
        "Para el fin: Salmo de David. Cántico de Jeremías y de Ezechiel "
        "para el pueblo trasportado al cautiverio, cuando empezaba á "
        "salir de él.",
        "",
        "tomo III, hoja 35",
    ),
    "Psalms 80:1": (
        "Para los lugares. Salmo para el mismo Asaph.",
        "Para el fin: Para los lugares. Salmo para el mismo Asaph.",
        "",
        "tomo III, hoja 45",
    ),
    "Psalms 87:1": (
        "Para los hijos de Coré, hasta el fin, sobre Maheleth: para "
        "cantarse alternativamente. Instruccion de Eman Ezrabita",
        "Cántico y Salmo. Para los hijos de Coré, hasta el fin, sobre "
        "Maheleth: para cantarse alternativamente. Instruccion de Eman "
        "Ezrabita",
        "",
        "tomo III, hoja 47",
    ),
    "Psalms 91:1": (
        "Para el día del sábado.",
        "Salmo y Cántico. Para el día del sábado.",
        "",
        "tomo III, hoja 49",
    ),
    # Títulos que el módulo no trae (v. 1 vacío) o trae ilegibles (Sal 71),
    # leídos enteros en la hoja: el «1.» impreso es solo la inscripción y el
    # cuerpo empieza en el «2.». Se transcribe el impreso tal cual, sin la
    # llamada de nota (Sal 107 «David ³.»), como en los Sal 50 y 51. El
    # cuerpo que también falta en el módulo (8:2, 38:2-3, 145:2-3) no se
    # rellena aquí.
    "Psalms 8:1": (
        "",
        "Al fin: para los lagares: Salmo de David.",
        "",
        "tomo III, hoja 12",
    ),
    "Psalms 38:1": (
        "",
        "Para el fin, á Idithun: Cántico de David.",
        "",
        "tomo III, hoja 25",
    ),
    "Psalms 71:1": (
        "Salmo 1 sobre &amp;gt;podas Salomon, er figura A de SA Christo. "
        "pt ANA",
        "Salmo sobre Salomon, figura de Christo.",
        "",
        "tomo III, hoja 38",
    ),
    "Psalms 107:1": (
        "",
        "Cántico y Salmo del mismo David.",
        "",
        "tomo III, hoja 55",
    ),
    "Psalms 108:1": (
        "",
        "Salmo de David: para el fin.",
        "",
        "tomo III, hoja 55",
    ),
    "Psalms 139:1": (
        "",
        "Para el fin: Salmo de David.",
        "",
        "tomo III, hoja 67",
    ),
    "Psalms 145:1": (
        "",
        "Aleluya: de Aggéo y de Zacharias.",
        "",
        "tomo III, hoja 69",
    ),
    # Título con cuerpo ajeno pegado (TRASLADOS). Sal 84 (hoja 46): «1. Para
    # el fin: Salmo para los hijos de Coré.» / «2. Oh Señor, tú has
    # derramado…»; el OCR juntó los dos y dejó el «2» dentro. Sal 9 (hoja
    # 13): tras el v. 21 el impreso abre la «Segunda parte, que es el Salmo
    # X segun los Hebreos» y numera de nuevo desde el 1; su «1. ¿Y por qué,
    # oh Señor…» es Vulgata 9:22 (Clementina «Ut quid, Domine, recessisti
    # longe…»), y el OCR lo pegó detrás del título 9:1. Los demás versos de
    # esa segunda parte siguen entrelazados en 9:2-21 (cuerpo, no título).
    "Psalms 84:1": (
        "Para el fin: Salmo para los hijos de Coré. 2 Oh Señor, tá has "
        "derramado la bendicion sobre tu tierra: tú has libertado del "
        "cautiverio á Jacob.",
        "Para el fin: Salmo para los hijos de Coré.",
        "",
        "tomo III, hoja 46",
    ),
    "Psalms 9:1": (
        "Para el fin: por los ocultos arcanos del Hijo: Salmo de — David,"
        " ¿Y por qué, oh Señor, te has retirado á lo lejos; y me has "
        "desamparado en el tiempo mas crítico, en la tribulaclon?",
        "Para el fin: por los ocultos arcanos del Hijo: Salmo de — David,",
        "",
        "tomo III, hoja 13",
    ),
    # Título y cuerpo en el mismo «1.» impreso (TORRES-PSALM-TITLES-101),
    # como el Sal 52:1: el título ocupa su propio renglón y el cuerpo sigue
    # en el renglón siguiente sin número. Se corta ahí, con el texto del
    # módulo tal cual (erratas «Dayid», «¡Hasta» por «¿Hasta», «titulo»,
    # la «A» suelta del Sal 22 quedan). Sal 13 (hoja 14): el OCR pegó
    # además el «2.» impreso («El Señor echó desde el cielo…») y dejó vacío
    # el 13:2; va a 13:2 (TRASLADOS).
    "Psalms 12:1": (
        "Para el fin: Salmo de Dayid. ¡Hasta cuándo, oh Señor, me has de "
        "tener en profundo olvido? ¿Hasta cuándo apartarás de mí tu "
        "rostro?",
        "Para el fin: Salmo de Dayid.",
        "¡Hasta cuándo, oh Señor, me has de tener en profundo olvido? "
        "¿Hasta cuándo apartarás de mí tu rostro?",
        "tomo III, hoja 13",
    ),
    "Psalms 13:1": (
        "Para el fin: Salmo de David. Dijo en su corazon el insensato: No"
        " hay Dios!, Los hombres se han corrompido, y se han hecho "
        "abominables por seguir sus pasiones: no hay quien obre bien, no "
        "hay uno siquiera. El Señor echó desde el cielo una mirada sobre "
        "los hijos de los hombres, para ver si habia uno que tuviese "
        "juicio, ó que buscase á Dios.",
        "Para el fin: Salmo de David.",
        "Dijo en su corazon el insensato: No hay Dios!, Los hombres se "
        "han corrompido, y se han hecho abominables por seguir sus "
        "pasiones: no hay quien obre bien, no hay uno siquiera.",
        "tomo III, hoja 14",
    ),
    "Psalms 14:1": (
        "Salmo de David. ¡Ah! Señor, ¿quién morará en tu celestial "
        "tabernáculo;O quién descansará en tu santo monte?",
        "Salmo de David.",
        "¡Ah! Señor, ¿quién morará en tu celestial tabernáculo;O quién "
        "descansará en tu santo monte?",
        "tomo III, hoja 14",
    ),
    "Psalms 15:1": (
        "Inscripcion de titulo: Del mismo David. Sálvame, oh Senor, pues "
        "tengo puesta en tí todw mi esperanza",
        "Inscripcion de titulo: Del mismo David.",
        "Sálvame, oh Senor, pues tengo puesta en tí todw mi esperanza",
        "tomo III, hoja 14",
    ),
    "Psalms 16:1": (
        "Oracion de David. Atiende, oh Señor, á mi justicia: acoge mis "
        "plegarias, Presta oidos á mi oracion; que no la pronuncio con "
        "labios hipócritas ó fraudulentos.",
        "Oracion de David.",
        "Atiende, oh Señor, á mi justicia: acoge mis plegarias, Presta "
        "oidos á mi oracion; que no la pronuncio con labios hipócritas ó "
        "fraudulentos.",
        "tomo III, hoja 14",
    ),
    "Psalms 22:1": (
        "Salmo de David. A El Señor me pastorea, nada me faltará.",
        "Salmo de David.",
        "A El Señor me pastorea, nada me faltará.",
        "tomo III, hoja 17",
    ),
    # Título y cuerpo compartidos, segundo lote. Sal 23-26 y 28: el título
    # va en su propio renglón y el cuerpo sigue sin número; se corta ahí
    # con el texto del módulo tal cual (la «Y» suelta del 23, «a! y» del
    # 25 y «Dayid» del 28 quedan). Sal 27 y 32: el impreso pone el título
    # DELANTE del «1.» («Salmo del mismo David. 1. Á tí…», «Salmo de David.
    # 1. Regocijaos…»), el OCR lo perdió y el v. 1 del módulo es todo
    # cuerpo: se antepone el título leído en la hoja y el v. 1 queda entero
    # como cuerpo.
    "Psalms 23:1": (
        "Para el primer dia de la semana: Salmo de David. Y Del Señor es "
        "la tierra, y cuanto ella contiene: el mundo, y todos sus "
        "habitadores",
        "Para el primer dia de la semana: Salmo de David.",
        "Y Del Señor es la tierra, y cuanto ella contiene: el mundo, y "
        "todos sus habitadores",
        "tomo III, hoja 17",
    ),
    "Psalms 24:1": (
        "Para el fin: Salmo de David. Á tí, oh Señor, he levantado mi "
        "espíritu.",
        "Para el fin: Salmo de David.",
        "Á tí, oh Señor, he levantado mi espíritu.",
        "tomo III, hoja 18",
    ),
    "Psalms 25:1": (
        "Para el fin: Salmo de David. a! y Oh, Señor, seas tú mi Juez, "
        "puesto que yo he procedido segun mi inocencia; y esperando en el"
        " Señor, no vacilaré.",
        "Para el fin: Salmo de David.",
        "a! y Oh, Señor, seas tú mi Juez, puesto que yo he procedido "
        "segun mi inocencia; y esperando en el Señor, no vacilaré.",
        "tomo III, hoja 18",
    ),
    "Psalms 26:1": (
        "Salmo de David antes de ser ungido. El Señor es mi luz y mi "
        "salvacion: ¡á quién he de temer yo? —El Señor es el defensor de "
        "mi vida: ¿quién me hará temblar?",
        "Salmo de David antes de ser ungido.",
        "El Señor es mi luz y mi salvacion: ¡á quién he de temer yo? —El "
        "Señor es el defensor de mi vida: ¿quién me hará temblar?",
        "tomo III, hoja 18",
    ),
    "Psalms 28:1": (
        "Salmo de Dayid, cuando se concluyó el Tabernáculo. Presentad al "
        "Señor, oh hijos de Dios, presentad al Señor corderos para el "
        "sacrificio.",
        "Salmo de Dayid, cuando se concluyó el Tabernáculo.",
        "Presentad al Señor, oh hijos de Dios, presentad al Señor "
        "corderos para el sacrificio.",
        "tomo III, hoja 19",
    ),
    "Psalms 27:1": (
        "A tí oh Señor, clamaré:no te hagas sordo á mis ruegos, Dios mio:"
        " no sea que no haciendo tú caso de mí, llegue yo á contarme con "
        "los que bajan al sepulero.",
        "Salmo del mismo David.",
        "A tí oh Señor, clamaré:no te hagas sordo á mis ruegos, Dios mio:"
        " no sea que no haciendo tú caso de mí, llegue yo á contarme con "
        "los que bajan al sepulero.",
        "tomo III, hoja 19",
    ),
    "Psalms 32:1": (
        "hRegoeijaos, oh justos, en el Señor: á los rectos de cordes á "
        "quienes les está bien el alabarle.",
        "Salmo de David.",
        "hRegoeijaos, oh justos, en el Señor: á los rectos de cordes á "
        "quienes les está bien el alabarle.",
        "tomo III, hoja 20",
    ),
    # Título y cuerpo compartidos, tercer lote: título en su propio renglón
    # del «1.» y cuerpo en el siguiente; corte ahí con el texto del módulo
    # tal cual (el «¡» suelto de 42 y 72 y «de d para» -- impreso «de ó
    # para» -- del 49 quedan).
    "Psalms 36:1": (
        "Salmo del mismo David. No envidies la. prosperidad de los "
        "malienos, ni tengas celos de los que obran la iniquidad",
        "Salmo del mismo David.",
        "No envidies la. prosperidad de los malienos, ni tengas celos de "
        "los que obran la iniquidad",
        "tomo III, hoja 24",
    ),
    "Psalms 42:1": (
        "¡Salmo de David. Júzgame tú, oh Dios, y toma en tus manos mi "
        "causa: líbrame de una gente impía, y del hombre inícuo y "
        "ensañador.",
        "¡Salmo de David.",
        "Júzgame tú, oh Dios, y toma en tus manos mi causa: líbrame de "
        "una gente impía, y del hombre inícuo y ensañador.",
        "tomo III, hoja 26",
    ),
    "Psalms 49:1": (
        "Salmo de d para Asaph. El Dios de los dioses, el Señor ha "
        "hablado, y ha convocado la tierra, —desde el Oriente hasta el "
        "Occidente.",
        "Salmo de d para Asaph.",
        "El Dios de los dioses, el Señor ha hablado, y ha convocado la "
        "tierra, —desde el Oriente hasta el Occidente.",
        "tomo III, hoja 29",
    ),
    "Psalms 72:1": (
        "¡ Salmo de Asaph. ¡Cuán bondadoso es Dios para Israél, para los "
        "que son de corazon recto!",
        "¡ Salmo de Asaph.",
        "¡Cuán bondadoso es Dios para Israél, para los que son de corazon"
        " recto!",
        "tomo III, hoja 39",
    ),
    "Psalms 73:1": (
        "Salmo de inteligencia de Asaph. ¿Y por qué, oh Dios, nos has "
        "desechado para siempre? ¿cómo se ha encendido tu furor contra "
        "las oyejitas que apacientas?",
        "Salmo de inteligencia de Asaph.",
        "¿Y por qué, oh Dios, nos has desechado para siempre? ¿cómo se ha"
        " encendido tu furor contra las oyejitas que apacientas?",
        "tomo III, hoja 39",
    ),
    # Título y cuerpo compartidos, cuarto lote. Sal 77, 78, 81, 86, 89:
    # título en su renglón del «1.», corte ahí con el texto del módulo tal
    # cual («¿mstruccion de Asaph,.», «4 los hijos» por «Á los hijos», «M
    # OYSÉs» quedan). Sal 85, 90, 92: título impreso DELANTE del «1.»,
    # perdido por el OCR; se antepone y el v. 1 queda entero como cuerpo.
    "Psalms 77:1": (
        "Inteligencia, ¿mstruccion de Asaph,. Escucha, pueblo mio, mi "
        "Ley; y ten atentos tus-oidos para percibir las palabras de mi "
        "boca.",
        "Inteligencia, ¿mstruccion de Asaph,.",
        "Escucha, pueblo mio, mi Ley; y ten atentos tus-oidos para "
        "percibir las palabras de mi boca.",
        "tomo III, hoja 41",
    ),
    "Psalms 78:1": (
        "Salmo de Asaph. Oh Dios, los Gentiles han entrado en tu heredad:"
        " han profanado tu santo templo: han dejado á Jerusalem tal como "
        "una barraca de hortelano",
        "Salmo de Asaph.",
        "Oh Dios, los Gentiles han entrado en tu heredad: han profanado "
        "tu santo templo: han dejado á Jerusalem tal como una barraca de "
        "hortelano",
        "tomo III, hoja 44",
    ),
    "Psalms 81:1": (
        "Salmo de Asaph. Presente está Dios en la reunion de los dioses "
        "de la tiezra $; y allí en medio de ellos juzga á los tales "
        "dioses.",
        "Salmo de Asaph.",
        "Presente está Dios en la reunion de los dioses de la tiezra $; y"
        " allí en medio de ellos juzga á los tales dioses.",
        "tomo III, hoja 45",
    ),
    "Psalms 86:1": (
        "4 los hijos de Coré. Salmo y Cántico, Sobre los montes santos "
        "está Jerusalem fundada.",
        "4 los hijos de Coré. Salmo y Cántico,",
        "Sobre los montes santos está Jerusalem fundada.",
        "tomo III, hoja 47",
    ),
    "Psalms 89:1": (
        "Oracion de M OYSÉs, varon de Dios. Señor, en todas épocas has "
        "sido tú nuestro amparo.",
        "Oracion de M OYSÉs, varon de Dios.",
        "Señor, en todas épocas has sido tú nuestro amparo.",
        "tomo III, hoja 48",
    ),
    "Psalms 85:1": (
        "Inelina, Señor, tu oido á máis ruegos, y escúchame: porque me "
        "hallo afligido y necesitado.",
        "Oracion del mismo David.",
        "Inelina, Señor, tu oido á máis ruegos, y escúchame: porque me "
        "hallo afligido y necesitado.",
        "tomo III, hoja 46",
    ),
    "Psalms 90:1": (
        "El que se acoge al asilo del Altísimo, descansará siem vre bajo "
        "la proteccion del Dios del cielo",
        "Alabanza y Cántico de David.",
        "El que se acoge al asilo del Altísimo, descansará siem vre bajo "
        "la proteccion del Dios del cielo",
        "tomo III, hoja 48",
    ),
    "Psalms 92:1": (
        "El Señor reinó: revistióse de gloria, armóse de fortaleza, y se "
        "cinó todo de ella. Asentó tambien firme la redondez de la "
        "tierra, y no será conmovida.",
        "Salmo y Cántico del mismo David, para la víspera del sábado, que"
        " es cuando fué criada la tierra.",
        "El Señor reinó: revistióse de gloria, armóse de fortaleza, y se "
        "cinó todo de ella. Asentó tambien firme la redondez de la "
        "tierra, y no será conmovida.",
        "tomo III, hoja 49",
    ),
    # Título y cuerpo compartidos, quinto lote. Sal 96, 97, 98, 100, 102:
    # título en su renglón del «1.», corte ahí. Sal 94: «Alabanza ó Cántico
    # del mismo David.» impreso DELANTE del «1.», perdido; el v. 1 entero es
    # cuerpo. Sal 95 (mixto): «Cántico del mismo David, cantado.» DELANTE
    # del «1.», perdido, y «1. Cuando se reedificó la Casa de Dios despues
    # de la cautividad.» detrás del número, que el módulo conserva: el
    # título es prefijo + esa parte, el cuerpo el resto del v. 1.
    "Psalms 96:1": (
        "Salmo de David, cuando fué restaurada su tierra. El Señor es el "
        "que reina: regocíjese la tierra; muestre su júbilo la multitud "
        "de Islas.",
        "Salmo de David, cuando fué restaurada su tierra.",
        "El Señor es el que reina: regocíjese la tierra; muestre su "
        "júbilo la multitud de Islas.",
        "tomo III, hoja 50",
    ),
    "Psalms 97:1": (
        "Salmo del mismo David. Cantad al Señor un cántico nuevo; porque "
        "ha hecho maravillas.—$Su diestra misma, y su santo brazo han "
        "obrado su salvacion.",
        "Salmo del mismo David.",
        "Cantad al Señor un cántico nuevo; porque ha hecho "
        "maravillas.—$Su diestra misma, y su santo brazo han obrado su "
        "salvacion.",
        "tomo III, hoja 50",
    ),
    "Psalms 98:1": (
        "Salmo del mismo David. Reina ya el Señor; estremézcanse los "
        "pueblos: reina ya quel que está sentado sobre querubines; "
        "agítese la tierra",
        "Salmo del mismo David.",
        "Reina ya el Señor; estremézcanse los pueblos: reina ya quel que "
        "está sentado sobre querubines; agítese la tierra",
        "tomo III, hoja 51",
    ),
    "Psalms 100:1": (
        "Salmo del mismo David. Cantaré, Señor, las alabanzas de tu "
        "misericordia y de tu jus-",
        "Salmo del mismo David.",
        "Cantaré, Señor, las alabanzas de tu misericordia y de tu jus-",
        "tomo III, hoja 51",
    ),
    "Psalms 102:1": (
        "Del mismo David. Bendice, oh alma mia, al Señor, y bendigan "
        "todas mis entrañas su santo Nombre.",
        "Del mismo David.",
        "Bendice, oh alma mia, al Señor, y bendigan todas mis entrañas su"
        " santo Nombre.",
        "tomo III, hoja 52",
    ),
    "Psalms 94:1": (
        "Venid, regocijémonos en el Señor: cantemos con júbilo las "
        "alabanzas del Dios, Salvador nuestro",
        "Alabanza ó Cántico del mismo David.",
        "Venid, regocijémonos en el Señor: cantemos con júbilo las "
        "alabanzas del Dios, Salvador nuestro",
        "tomo III, hoja 50",
    ),
    "Psalms 95:1": (
        "Cuando se reedificó la Casa de Dios despues de la cautividad. "
        "Cantad al Señor un cántico nuevo: regiones todas de la tierra, "
        "cantad al señor",
        "Cántico del mismo David, cantado. Cuando se reedificó la Casa de"
        " Dios despues de la cautividad.",
        "Cantad al Señor un cántico nuevo: regiones todas de la tierra, "
        "cantad al señor",
        "tomo III, hoja 50",
    ),
    # Título y cuerpo compartidos, sexto lote. Sal 103, 109: título en su
    # renglón del «1.», corte ahí (la «D» suelta del 109 queda en el
    # cuerpo). Sal 104, 105, 106, 110, 111: «Aleluya» (111: «Aleluya: del
    # regreso de Aggéo y de Zacharias.») impreso DELANTE del «1.», perdido;
    # se antepone sin la llamada de nota y el v. 1 entero queda como
    # cuerpo.
    "Psalms 103:1": (
        "Del mismo David. Oh alma mia, bendice al Señor. Senor Dios mio, "
        "tú te has engrandecido mucho en gran manera. —Revestido te has "
        "de! erloria y de majestad",
        "Del mismo David.",
        "Oh alma mia, bendice al Señor. Senor Dios mio, tú te has "
        "engrandecido mucho en gran manera. —Revestido te has de! erloria"
        " y de majestad",
        "tomo III, hoja 52",
    ),
    "Psalms 109:1": (
        "Salmo de David. D El Señor dijo á mi Señor: Siéntate á mi "
        "diestra %;—mientras que yo pongo Já tus enemigos por tarima de "
        "tus piés.",
        "Salmo de David.",
        "D El Señor dijo á mi Señor: Siéntate á mi diestra %;—mientras "
        "que yo pongo Já tus enemigos por tarima de tus piés.",
        "tomo III, hoja 58",
    ),
    "Psalms 104:1": (
        "Alabad al Señor, é Invocad su Nombre: predicad entre las "
        "naciones sus admirables obras.",
        "Aleluya.",
        "Alabad al Señor, é Invocad su Nombre: predicad entre las "
        "naciones sus admirables obras.",
        "tomo III, hoja 53",
    ),
    "Psalms 105:1": (
        "Alabad al Señor porque es tan bueno, porque es eterna su "
        "misericordia",
        "Aleluya.",
        "Alabad al Señor porque es tan bueno, porque es eterna su "
        "misericordia",
        "tomo III, hoja 54",
    ),
    "Psalms 106:1": (
        "Alabad al Señor, porque es tan bueno, porque es eterna su "
        "misericordia.",
        "Aleluya.",
        "Alabad al Señor, porque es tan bueno, porque es eterna su "
        "misericordia.",
        "tomo III, hoja 54",
    ),
    "Psalms 110:1": (
        "Oh Señor, loarte he con todo mi corazon en la sociedarl de los "
        "justos, y en la /glesia ó congregacion.",
        "Aleluya.",
        "Oh Señor, loarte he con todo mi corazon en la sociedarl de los "
        "justos, y en la /glesia ó congregacion.",
        "tomo III, hoja 58",
    ),
    "Psalms 111:1": (
        "Bienayenturado el hombre que teme al Señor: y que toda su "
        "aficion la pone en cumplir sus mandamientos.",
        "Aleluya: del regreso de Aggéo y de Zacharias.",
        "Bienayenturado el hombre que teme al Señor: y que toda su "
        "aficion la pone en cumplir sus mandamientos.",
        "tomo III, hoja 58",
    ),
    # Séptimo lote (Sal 114-120). «Aleluya.» (120: «Cántico gradual.»)
    # impreso DELANTE del primer número, perdido; se antepone y el v. 1
    # entero queda como cuerpo. Sal 115: el impreso numera 10-19 a la
    # hebrea (continúa el 114 hebreo), que son Vulgata 115:1-10; el
    # «Aleluya.» va delante del «10.» y el 115:1 del módulo es ese verso
    # (con el titulillo «85 SALMOS.» colado, que se queda). Sal 118: tras
    # «Aleluya.» va la letra «ALEPH.», rótulo de estrofa, que no es título.
    # Sal 119: título en su renglón del «1.», corte ahí («0 gradual. eme»
    # del OCR: «0» queda en el título, «eme» en el cuerpo; lo que se
    # escribe es el impreso, ver IMPRESO).
    "Psalms 114:1": (
        "Amé al Señor, seguro de que oirá la voz de mi oracion.",
        "Aleluya.",
        "Amé al Señor, seguro de que oirá la voz de mi oracion.",
        "tomo III, hoja 59",
    ),
    "Psalms 115:1": (
        "Creí d Dios; por eso hablé contado 5, aunque me ví reducido al "
        "mayor abatimiento, 85 SALMOS.",
        "Aleluya.",
        "Creí d Dios; por eso hablé contado 5, aunque me ví reducido al "
        "mayor abatimiento, 85 SALMOS.",
        "tomo III, hoja 59",
    ),
    "Psalms 116:1": (
        "Alabad al Señor, naciones todas de la tierra: pueblos todos "
        "cantad sus alabanzas.",
        "Aleluya.",
        "Alabad al Señor, naciones todas de la tierra: pueblos todos "
        "cantad sus alabanzas.",
        "tomo III, hoja 60",
    ),
    "Psalms 117:1": (
        "Alabad al Señor, porque es tan bueno; porque hace brillar "
        "eternamente su misericordia",
        "Aleluya.",
        "Alabad al Señor, porque es tan bueno; porque hace brillar "
        "eternamente su misericordia",
        "tomo III, hoja 60",
    ),
    "Psalms 118:1": (
        "Bienaventurados los que proceden sin man cilla, los que caminan "
        "segun la Ley del Señor.",
        "Aleluya.",
        "Bienaventurados los que proceden sin man cilla, los que caminan "
        "segun la Ley del Señor.",
        "tomo III, hoja 60",
    ),
    "Psalms 120:1": (
        "Aleé mis ojos hácia los montes de Jerusalem, de donde me ha de "
        "venir el socorro.",
        "Cántico gradual.",
        "Aleé mis ojos hácia los montes de Jerusalem, de donde me ha de "
        "venir el socorro.",
        "tomo III, hoja 63",
    ),
    "Psalms 119:1": (
        "Cántico de los grados, 0 gradual. eme Clamé al Señor en mi "
        "tribulacion, y me atendió.",
        "Cántico de los grados, 0 gradual.",
        "eme Clamé al Señor en mi tribulacion, y me atendió.",
        "tomo III, hoja 63",
    ),
    # Octavo lote (Sal 121-128, «Cántico gradual»). Sal 121, 124-128:
    # «1. Cántico gradual[ de Salomon].» en su renglón, corte ahí (124 trae
    # «gradual,» del OCR). Sal 122: «Cántico gradual.» DELANTE del «1.»,
    # perdido; v. 1 entero como cuerpo. Sal 128:1 lleva además pegado el
    # v. 2 impreso («… desde 2 Muchas veces…», 128:2 vacío): el título se
    # marca igual y ese cuerpo queda sin tocar, para su propia tarea.
    "Psalms 121:1": (
        "Cántico gradual. Gran contento tuve cuando se me dijo: remos á "
        "la Casa del Senor",
        "Cántico gradual.",
        "Gran contento tuve cuando se me dijo: remos á la Casa del Senor",
        "tomo III, hoja 63",
    ),
    "Psalms 124:1": (
        "Cántico gradual, Los que ponen en el Señor su confianza estarán "
        "firmes como el monte de Sion: nunca jamás será derrocado el "
        "morador",
        "Cántico gradual,",
        "Los que ponen en el Señor su confianza estarán firmes como el "
        "monte de Sion: nunca jamás será derrocado el morador",
        "tomo III, hoja 64",
    ),
    "Psalms 125:1": (
        "Cántico gradual. Cuando el Señor hará volver á Sion los "
        "cautivos, será 1ndecible nuestro consuelo",
        "Cántico gradual.",
        "Cuando el Señor hará volver á Sion los cautivos, será 1ndecible "
        "nuestro consuelo",
        "tomo III, hoja 64",
    ),
    "Psalms 126:1": (
        "Cántico gradual de Salomon. SI el Señor no es el que edifica la "
        "casa, en vano se fatigan los que la fabrican, —Si el Señor no "
        "guarda la eindad, inútil mente se desvela el que la guarda.",
        "Cántico gradual de Salomon.",
        "SI el Señor no es el que edifica la casa, en vano se fatigan los"
        " que la fabrican, —Si el Señor no guarda la eindad, inútil mente"
        " se desvela el que la guarda.",
        "tomo III, hoja 64",
    ),
    "Psalms 127:1": (
        "Cántico gradual. Bienaventurados todos aquellos que temen al "
        "Señor, que andan por sus sentos eaminos.",
        "Cántico gradual.",
        "Bienaventurados todos aquellos que temen al Señor, que andan por"
        " sus sentos eaminos.",
        "tomo III, hoja 64",
    ),
    "Psalms 128:1": (
        "Cántico gradual. Muchas veces me han asaltado os enemigos desde "
        "mi tierna edad; dígalo ahora Israél: desde 2 Muchas veces me han"
        " asaltado mi tierna edad; pero no han podido conmigo.",
        "Cántico gradual.",
        "Muchas veces me han asaltado os enemigos desde mi tierna edad; "
        "dígalo ahora Israél: desde 2 Muchas veces me han asaltado mi "
        "tierna edad; pero no han podido conmigo.",
        "tomo III, hoja 65",
    ),
    "Psalms 122:1": (
        "Átí Señor, que habitas en los cielos, levanté mis ojos.",
        "Cántico gradual.",
        "Átí Señor, que habitas en los cielos, levanté mis ojos.",
        "tomo III, hoja 64",
    ),
    # Noveno lote (Sal 129-150, los que faltaban de verso compartido) y los
    # apartados cuya inscripción se puede marcar sin mover cuerpo a otro
    # salmo. Leído en el jp2 del tomo III (1882), hojas citadas. El cuerpo
    # que el módulo ya tenía se conserva, erratas incluidas.
    # Corte en el renglón del «1.»: el título ya está al principio del verso.
    "Psalms 129:1": (
        "Cántico gradual. Desde lo mas profundo clamé á tí, oh Señor.",
        "Cántico gradual.",
        "Desde lo mas profundo clamé á tí, oh Señor.",
        "tomo III, hoja 65",
    ),
    "Psalms 131:1": (
        "Cántico gradual. Acuérdate de David, oh Señor, y de toda su gran "
        "mansedumbre",
        "Cántico gradual.",
        "Acuérdate de David, oh Señor, y de toda su gran mansedumbre",
        "tomo III, hoja 65",
    ),
    "Psalms 132:1": (
        "Cántico gradual de David. ¡Oh cuán buena y cuán dulce cosa es el "
        "vivir los hermanos 3 en mútua union!",
        "Cántico gradual de David.",
        "¡Oh cuán buena y cuán dulce cosa es el vivir los hermanos 3 en "
        "mútua union!",
        "tomo III, hoja 65",
    ),
    "Psalms 134:1": (
        "Aleluya. Alabad el Nombre del Señor: tributadle alabanzas vosotros "
        "siervos suyos",
        "Aleluya.",
        "Alabad el Nombre del Señor: tributadle alabanzas vosotros siervos "
        "suyos",
        "tomo III, hoja 66",
    ),
    "Psalms 135:1": (
        "Aleluya. Alabad al Señor, porque es infinitamente bueno: Porque es "
        "eterna su misericordia",
        "Aleluya.",
        "Alabad al Señor, porque es infinitamente bueno: Porque es eterna "
        "su misericordia",
        "tomo III, hoja 66",
    ),
    "Psalms 137:1": (
        "Del mismo David. Te alabaré, Señor, con todo mi corazon; porque "
        "oiste las peticiones de mi boca.— En presencia de los ángeles te "
        "cantaré himnos",
        "Del mismo David.",
        "Te alabaré, Señor, con todo mi corazon; porque oiste las "
        "peticiones de mi boca.— En presencia de los ángeles te cantaré "
        "himnos",
        "tomo III, hoja 67",
    ),
    "Psalms 138:1": (
        "Para el fin: Salmo de David. Oh Señor, tú has hecho prueba de mí, "
        "y me tienes bien conocido.",
        "Para el fin: Salmo de David.",
        "Oh Señor, tú has hecho prueba de mí, y me tienes bien conocido.",
        "tomo III, hoja 67",
    ),
    "Psalms 140:1": (
        "Salmo de David, Señor, á tí he clamado, óyeme benigno: atiende á "
        "mi voz, cuando hácta ti la dirijo.",
        "Salmo de David,",
        "Señor, á tí he clamado, óyeme benigno: atiende á mi voz, cuando "
        "hácta ti la dirijo.",
        "tomo III, hoja 68",
    ),
    "Psalms 144:1": (
        "Alabanza inspirada al mismo David. Ensalzarte he, oh Dios, y Rey "
        "mio, y bendeciré tu santo Nombre desde ahora y por los siglos de "
        "los siglos",
        "Alabanza inspirada al mismo David.",
        "Ensalzarte he, oh Dios, y Rey mio, y bendeciré tu santo Nombre "
        "desde ahora y por los siglos de los siglos",
        "tomo III, hoja 69",
    ),
    "Psalms 148:1": (
        "Alehiya. Alabad al Señor vosotros que estais en los cielos; "
        "alabadle los que estais en las alturas",
        "Alehiya.",
        "Alabad al Señor vosotros que estais en los cielos; alabadle los "
        "que estais en las alturas",
        "tomo III, hoja 72",
    ),
    "Psalms 149:1": (
        "Aleluya. Cantad al Señor un cántico nuevo: resuenen sus loores en "
        "la reunion de los santos.",
        "Aleluya.",
        "Cantad al Señor un cántico nuevo: resuenen sus loores en la "
        "reunion de los santos.",
        "tomo III, hoja 72",
    ),
    "Psalms 150:1": (
        "Aleluya, Alabad al Senor que reside en su celestit! Santuario: "
        "alabadle sentado en el firmamento d trono de su poder",
        "Aleluya,",
        "Alabad al Senor que reside en su celestit! Santuario: alabadle "
        "sentado en el firmamento d trono de su poder",
        "tomo III, hoja 72",
    ),
    # Título en su renglón (o delante del «1.») y el módulo no lo trae: se
    # antepone y el v. 1 entero queda de cuerpo.
    "Psalms 130:1": (
        "Oh Señor, no se ha engreido mi corazon, ni mis ojos se han "
        "mostrado altivos. No he aspirado á cosas grandes, ni á cosas "
        "elevadas sobre mi capacidad.",
        "Cántico gradual de David.",
        "Oh Señor, no se ha engreido mi corazon, ni mis ojos se han "
        "mostrado altivos. No he aspirado á cosas grandes, ni á cosas "
        "elevadas sobre mi capacidad.",
        "tomo III, hoja 65",
    ),
    "Psalms 136:1": (
        "En las márgenes de los rios del pais de Babylonia, allí hnos "
        "sentábamos, y nos poníamos á llorar, acordándonos de 47, oh Sion",
        "Salmo de David, para Jeremías.",
        "En las márgenes de los rios del pais de Babylonia, allí hnos "
        "sentábamos, y nos poníamos á llorar, acordándonos de 47, oh Sion",
        "tomo III, hoja 66",
    ),
    # El impreso acaba «á tu Dios.»; el OCR perdió la última palabra
    # (TORRES-PSALM-GLUE-101).
    "Psalms 147:1": (
        "Alaba al Señor, oh Jerusalem; alaba, oh Sion, á tu",
        "Aleluya.",
        "Alaba al Señor, oh Jerusalem; alaba, oh Sion, á tu Dios.",
        "tomo III, hoja 69",
    ),
    "Psalms 31:1": (
        "Felices aquellos á quienes se han perdonado sus iniquidades, Pi y "
        "se han borrado sus pecados. o el hombre á quien el Señor no "
        "arguye e de pecado; y cuya alma se halla exenta de dolo.",
        "Del mismo David, Salmo de inteligencia.",
        "Felices aquellos á quienes se han perdonado sus iniquidades, Pi y "
        "se han borrado sus pecados. o el hombre á quien el Señor no "
        "arguye e de pecado; y cuya alma se halla exenta de dolo.",
        "tomo III, hoja 20",
    ),
    "Psalms 112:1": (
        "Alabad, oh jóvenes, al Señor: dad loores al Nombre del Señor, "
        "Cuando Israél salió de Esypto, al partir la casa de Jacob de en "
        "medio de aquel pueblo extranjero",
        "Aleluya.",
        "Alabad, oh jóvenes, al Señor: dad loores al Nombre del Señor, "
        "Cuando Israél salió de Esypto, al partir la casa de Jacob de en "
        "medio de aquel pueblo extranjero",
        "tomo III, hoja 59",
    ),
    # Prefijo perdido y el resto de la inscripción sigue en el módulo.
    "Psalms 142:1": (
        "E. Cuando le perseguia su hijo Absalom 7, Oh Señor, escucha mi "
        "oracion; presta oidos á mi súplica, segun la verdad de tus "
        "promesas: óyeme por ti misericordia",
        "Salmo de David: E. Cuando le perseguia su hijo Absalom 7,",
        "Oh Señor, escucha mi oracion; presta oidos á mi súplica, segun la "
        "verdad de tus promesas: óyeme por ti misericordia",
        "tomo III, hoja 68",
    ),
    "Psalms 143:1": (
        "Contra Goliath. Bendito sea el Señor Dios mio, que adiestra mis "
        "manos para la pelea, y mis dedos para manejar las armas. sálvame "
        "aora,—y sácame de las garras de estos extranjeros, de cuya boca "
        "no sale sino vanidad y mentira, y cuyas manos están llenas de "
        "iniquidad",
        "Salmo de David: Contra Goliath.",
        "Bendito sea el Señor Dios mio, que adiestra mis manos para la "
        "pelea, y mis dedos para manejar las armas. sálvame aora,—y sácame "
        "de las garras de estos extranjeros, de cuya boca no sale sino "
        "vanidad y mentira, y cuyas manos están llenas de iniquidad",
        "tomo III, hoja 68",
    ),
    "Psalms 65:1": (
        "Salmo y Cántico de la Resurreccion, Moradores todos de la tierra, "
        "dirigid á Dios voces de y Júbilo: e 2, Cantad salmos á su Nombre, "
        "tributadle gloriosas alabanzas.",
        "Para el fin: Salmo y Cántico de la Resurreccion,",
        "Moradores todos de la tierra, dirigid á Dios voces de y Júbilo: "
        "e 2, Cantad salmos á su Nombre, tributadle gloriosas alabanzas.",
        "tomo III, hoja 35",
    ),
    # Inscripción ilegible en el módulo; se sustituye por la de la hoja 38.
    # El cuerpo («En tí, oh Señor…») es el que ya había. SUSTITUYE_BASURA.
    "Psalms 70:1": (
        "De los hijos.. de ] Jonadab, z y de los pri-. ImMeéros cautivos. "
        "En tí, oh Señor, tengo puesta mi esperanza: no sea yo para siempre "
        "confundido",
        "Salmo de David: De los hijos de Jonadab, y de los primeros "
        "cautivos.",
        "En tí, oh Señor, tengo puesta mi esperanza: no sea yo para siempre "
        "confundido",
        "tomo III, hoja 38",
    ),
    # El v. 1 del módulo repite entero el 132:1 y detrás trae el título
    # («Cántico gracual.») y el cuerpo. La copia se descarta: el 132:1 no
    # se toca. Ver DUPLICADO_DELANTE.
    "Psalms 133:1": (
        "Cántico gradual de David. ¡Oh cuán buena y cuán dulce cosa es el "
        "vivir los hermanos 3 en mútua union! Cántico gracual. Ea pues, "
        "bendecid al Señor ahora mismo, vosotros todos, oh siervos del "
        "Señor. Vosotros los que asistís en la Casa del Señor, en los "
        "atrios del templo de nuestro Dios",
        "Cántico gracual.",
        "Ea pues, bendecid al Señor ahora mismo, vosotros todos, oh "
        "siervos del Señor. Vosotros los que asistís en la Casa del Señor, "
        "en los atrios del templo de nuestro Dios",
        "tomo III, hoja 65",
    ),
    # El módulo traía el verso vacío: título y cuerpo del «1.» salen del
    # impreso. El v. 2, si existe, no se toca.
    "Psalms 34:1": (
        "",
        "Salmo del mismo David.",
        "Juzga, oh Señor, á los que me dañan: bate á los que pelean contra "
        "mí.",
        "tomo III, hoja 21",
    ),
    "Psalms 123:1": (
        "",
        "Cántico gradual.",
        "Á no haber estado el Señor con nosotros, confiéselo ahora Israél,",
        "tomo III, hoja 64",
    ),
    "Psalms 146:1": (
        "",
        "Aleluya.",
        "Alabad al Señor; porque justa cosa es cantarle himnos. Cántese á "
        "nuestro Dios un grato y digno cántico.",
        "tomo III, hoja 69",
    ),
    "Psalms 93:1": (
        "",
        "Salmo del mismo David, para el cuarto dia de la semana.",
        "El Señor ó Jehovah, es el Dios de las venganzas: y el Dios de las "
        "venganzas ha obrado con independiente libertad.",
        "tomo III, hoja 49",
    ),
}


# Títulos cuyo texto viejo en el módulo es basura de OCR y se sustituye
# entero por el impreso (el resto de TITULOS conserva lo que había). Cada
# uno, con lo que traía: Sal 71:1 «Salmo 1 sobre &amp;gt;podas Salomon, er
# figura A de SA Christo. pt ANA» (tomo III, hoja 38: «1. Salmo sobre
# Salomon, figura de Christo.»). Sal 70:1 trae la inscripción hecha
# pedazos («De los hijos.. de ] Jonadab…»); el impreso, hoja 38, dice
# «Salmo de David: 1. De los hijos de Jonadab, y de los primeros
# cautivos.» El cuerpo que sigue («En tí, oh Señor…») se conserva.
SUSTITUYE_BASURA = {"Psalms 71:1", "Psalms 70:1"}

# Título (TITULOS) seguido de texto de otro versículo, vacío en el módulo,
# al que se traslada (entrada de CORRECCIONES de "" a ese texto).
# origen -> destino. cambios() exige que el texto viejo del origen sea
# exactamente título [+ cuerpo propio] + texto del destino, con separadores
# entre medias: nada se pierde, nada se duplica. El separador puede llevar
# el número que el OCR dejó dentro («… de Coré. 2 Oh Señor»).
TRASLADOS = {"Psalms 84:1": "Psalms 84:2", "Psalms 9:1": "Psalms 9:22",
             "Psalms 13:1": "Psalms 13:2"}
# Copia exacta de otro versículo, pegada delante del título. El otro
# versículo ya la tiene y no está vacío, así que no se traslada: se
# descarta la copia. ref -> versículo del que es copia.
DUPLICADO_DELANTE = {"Psalms 133:1": "Psalms 132:1"}
RE_SEPARADOR = re.compile(r"\s+(?:\d{1,3}\s+)?")


def _comprueba_traslados():
    for origen, destino in TRASLADOS.items():
        viejo, titulo, cuerpo, _h = TITULOS[origen]
        vacio, trasladado, _h2 = CORRECCIONES[destino]
        if vacio != "" or not trasladado:
            raise ValueError(f"{origen} -> {destino}: traslado mal formado")
        # Sin cuerpo propio (Sal 9, 84): título + trasladado. Con cuerpo
        # propio en el mismo «1.» (Sal 13): título + cuerpo + trasladado.
        piezas = [titulo] + ([cuerpo] if cuerpo else []) + [trasladado]
        resto = viejo
        for k, pieza in enumerate(piezas):
            if k:
                sep = RE_SEPARADOR.match(resto)
                if not sep:
                    raise ValueError(f"{origen} -> {destino}: falta "
                                     "separador")
                resto = resto[sep.end():]
            if not resto.startswith(pieza):
                raise ValueError(f"{origen} -> {destino}: el texto viejo no "
                                 "es título + cuerpo + texto trasladado")
            resto = resto[len(pieza):]
        if resto:
            raise ValueError(f"{origen} -> {destino}: sobra texto «{resto}»")


def _comprueba_duplicados():
    for ref, otro in DUPLICADO_DELANTE.items():
        viejo, titulo, cuerpo, _h = TITULOS[ref]
        otro_viejo = TITULOS[otro][0]
        if viejo != f"{otro_viejo} {titulo} {cuerpo}":
            raise ValueError(f"{ref}: la copia de {otro} no es exacta")


def _comprueba_fusiones():
    vistos = set()
    for origen, destino, left, mid, right in FUSIONES:
        if origen in vistos or destino in vistos or origen == destino:
            raise ValueError(f"fusión repetida: {origen} {destino}")
        vistos.add(origen)
        vistos.add(destino)
        if origen in CORRECCIONES or origen in TITULOS:
            raise ValueError(f"{origen} ya está corregido")
        if destino in CORRECCIONES or destino in TITULOS:
            raise ValueError(f"{destino} ya está corregido")
        if not left or not right or not mid.strip():
            raise ValueError(f"{origen}: fusión vacía")
        if f"{left}{mid}{right}" != f"{left}{mid}{right}":
            raise ValueError(origen)


# Título y cuerpo tal como los trae el impreso, cuando el OCR de una entrada
# de TITULOS tiene erratas (TORRES-PSALM-TITLE-OCR-101). TITULOS guarda lo
# que el OCR leyó, porque con eso se comprueba el corte; esto es lo que se
# escribe. Solo erratas pequeñas del título y ruido delante del cuerpo.
IMPRESO = {
    # «0 gradual. eme»: «ó gradual.» en cursiva; «eme» es ruido del grabado.
    "Psalms 119:1": ("Cántico de los grados, ó gradual.",
                     "Clamé al Señor en mi tribulacion, y me atendió.",
                     "tomo III, hoja 63"),
    "Psalms 124:1": ("Cántico gradual.", None, "tomo III, hoja 64"),
    "Psalms 133:1": ("Cántico gradual.", None, "tomo III, hoja 65"),
    # Columnas fundidas (columnas_fundidas.py): el resto del renglón es el
    # Sal 113:1, que pasa a su verso; y la cabecera «85 SALMOS.».
    "Psalms 112:1": ("Aleluya.", "Alabad, oh jóvenes, al Señor: dad loores al "
                     "Nombre del Señor,", "tomo III, hoja 59"),
    "Psalms 115:1": ("Aleluya.", "Creí d Dios; por eso hablé contado 5, "
                     "aunque me ví reducido al mayor abatimiento",
                     "tomo III, hoja 59"),
}
# Diferencias que admite un título de IMPRESO frente al OCR.
MAX_ERRATAS_TITULO = 2


def _distancia(a, b):
    fila = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        previa, fila[0] = fila[0], i
        for j, cb in enumerate(b, 1):
            previa, fila[j] = fila[j], min(fila[j] + 1, fila[j - 1] + 1,
                                           previa + (ca != cb))
    return fila[-1]


def impreso(ref):
    """(título, cuerpo) que se escriben para una entrada de TITULOS."""
    _viejo, titulo, cuerpo, _hoja = TITULOS[ref]
    if ref not in IMPRESO:
        return titulo, cuerpo
    t, c, _h = IMPRESO[ref]
    return t, cuerpo if c is None else c


def _comprueba_impreso():
    for ref, (t, c, _h) in IMPRESO.items():
        _viejo, titulo, cuerpo, _hoja = TITULOS[ref]
        if _distancia(t, titulo) > MAX_ERRATAS_TITULO:
            raise ValueError(f"{ref}: el título impreso no es una errata")
        # El cuerpo solo puede perder ruido o un trozo ajeno por los bordes.
        if c is not None and (not c or c not in cuerpo):
            raise ValueError(f"{ref}: el cuerpo impreso no es el del OCR")


# Lo que un parche anterior dejó en un verso cuya corrección ha cambiado
# después: se acepta como punto de partida, así el parche vale también sobre
# el módulo instalado. Sal 147:1 recuperó «Dios.» en TORRES-PSALM-GLUE-101.
ANTERIORES = {
    "Psalms 147:1": marca_titulo(
        "Aleluya.", "Alaba al Señor, oh Jerusalem; alaba, oh Sion, á tu"),
}
# Y los títulos que IMPRESO corrige, tal como los dejaba el parche.
ANTERIORES.update({ref: marca_titulo(*TITULOS[ref][1:3]) for ref in IMPRESO})


# Caracteres que puede haber entre el texto del versículo y el encabezado
# («o SALMO», «3 er Ea, a, P al k SALMO»): ruido del grabado.
RUIDO_DELANTE = 25


def _comprueba_cabeceras():
    """Cada entrada de CABECERAS solo quita (ver cabeceras_pegadas.py)."""
    for ref, (viejo, nuevo, _h) in CABECERAS.items():
        if ref in CORRECCIONES or ref in TITULOS:
            raise ValueError(f"{ref} ya está corregido")
        if ref in TRASLADADOS:
            origen = CABECERAS[TRASLADADOS[ref]]
            if viejo != "" or not nuevo or nuevo not in origen[0] \
                    or nuevo in origen[1]:
                raise ValueError(f"{ref}: traslado mal formado")
            continue
        pre = 0
        while pre < min(len(viejo), len(nuevo)) and viejo[pre] == nuevo[pre]:
            pre += 1
        suf = 0
        while suf < min(len(viejo), len(nuevo)) - pre and \
                viejo[-1 - suf] == nuevo[-1 - suf]:
            suf += 1
        quitado = viejo[pre:len(viejo) - suf]
        puesto = nuevo[pre:len(nuevo) - suf]
        if ref in REPETIDOS:
            if puesto or not quitado.strip():
                raise ValueError(f"{ref}: repetido mal formado")
            continue
        # Lo quitado empieza en el encabezado: delante solo cabe el ruido
        # del grabado o, declarada, una nota al pie.
        delante = quitado.find("SALMO")
        if delante < 0:
            raise ValueError(f"{ref}: lo quitado no es un encabezado")
        if delante > RUIDO_DELANTE and NOTAS.get(ref, "\0") not in quitado:
            raise ValueError(f"{ref}: se quita texto antes del encabezado")
        if puesto not in ("", ".") and \
                not (puesto and puesto in AÑADIDOS.get(ref, "")):
            raise ValueError(f"{ref}: añade «{puesto}» sin declararlo")


def _plano_columnas(texto):
    """Texto sin marcado ni blancos, para comparar trozos."""
    return re.sub(r"\s+", "", re.sub(r"<[^>]+>", "", texto))


def _comprueba_columnas():
    """Cada verso nuevo de COLUMNAS se hace con trozos de los viejos."""
    viejos = _plano_columnas(" ".join(v for v, _n, _h in COLUMNAS.values())
                             + " " + " ".join(TITULOS[r][0] for r in
                                              ("Psalms 112:1",)))
    for ref, (_viejo, nuevo, _h) in COLUMNAS.items():
        if ref in CORRECCIONES or ref in TITULOS or ref in CABECERAS:
            raise ValueError(f"{ref} ya está corregido")
        piezas = [t for t in re.split(r"<[^>]+>", nuevo) if t.strip()]
        for pieza in piezas:
            plano = _plano_columnas(pieza)
            # Solo se admite cambiar el signo final («,» por «.»).
            if plano not in viejos and plano[:-1] not in viejos:
                raise ValueError(f"{ref}: texto que no estaba: «{pieza}»")


def cambios():
    """ref -> (texto viejo, texto nuevo): CORRECCIONES, CABECERAS, TITULOS
    y fusiones."""
    _comprueba_traslados()
    _comprueba_duplicados()
    _comprueba_fusiones()
    _comprueba_cabeceras()
    _comprueba_columnas()
    todos = {ref: (viejo, nuevo) for ref, (viejo, nuevo, _hoja)
             in list(CORRECCIONES.items()) + list(CABECERAS.items())
             + list(COLUMNAS.items())}
    _comprueba_impreso()
    for ref, (viejo, _titulo, _cuerpo, _hoja) in TITULOS.items():
        if ref in todos:
            raise ValueError(f"{ref} está en CORRECCIONES y en TITULOS")
        todos[ref] = (viejo, marca_titulo(*impreso(ref)))
    for origen, destino, left, mid, right in FUSIONES:
        if origen in todos or destino in todos:
            raise ValueError(f"fusión pisa otra corrección: {origen}")
        todos[origen] = (f"{left}{mid}{right}", left)
        todos[destino] = ("", right)
    return todos


def lee_imp(texto):
    """[(clave, cuerpo)] en el orden del fichero."""
    entradas, clave, cuerpo = [], None, []
    # escribe_imp termina cada cuerpo con su salto; el último no abre línea.
    if texto.endswith("\n"):
        texto = texto[:-1]
    for linea in texto.split("\n"):
        if linea.startswith("$$$"):
            if clave is not None:
                entradas.append((clave, "\n".join(cuerpo)))
            clave, cuerpo = linea[3:], []
        elif clave is not None:
            cuerpo.append(linea)
    if clave is not None:
        entradas.append((clave, "\n".join(cuerpo)))
    return entradas


def escribe_imp(entradas):
    return "".join(f"$$${clave}\n{cuerpo}\n" for clave, cuerpo in entradas)


def aplica(entradas, autorizados):
    """Sustituye solo las entradas autorizadas cuyo texto es el esperado.

    Una entrada ya corregida se deja como está; una con otro texto, o una
    clave autorizada que no aparece, detiene el parche (ValueError). El
    texto viejo puede ser una tupla de lecturas aceptadas (ANTERIORES).
    """
    pendientes = dict(autorizados)
    nuevas = []
    for clave, cuerpo in entradas:
        if clave in pendientes:
            viejo, nuevo = pendientes.pop(clave)
            viejos = viejo if isinstance(viejo, tuple) else (viejo,)
            if cuerpo == nuevo:
                print(f"  ya corregido: {clave}")
            elif cuerpo not in viejos:
                raise ValueError(f"{clave}: el módulo no trae el texto "
                                 f"esperado:\n  {cuerpo!r}")
            else:
                print(f"  corrige {clave}")
            cuerpo = nuevo
        nuevas.append((clave, cuerpo))
    if pendientes:
        raise ValueError(f"claves no encontradas: {sorted(pendientes)}")
    return nuevas


def autoriza(entradas):
    """cambios() sobre el módulo `entradas`, y el léxico de entidades.py.

    Los restos de entidades escapadas dos veces salen de todo el módulo
    antes de aplicar las correcciones (quita_restos), así que el texto
    viejo y el nuevo de cada corrección se comparan ya sin restos: vale
    igual sobre un módulo anterior a TORRES-ENTITY-101 que sobre uno
    posterior (1 Sam 19:20-21 los traían en el texto de FUSIONES).
    """
    frec = entidades.lexico(c for _, c in entradas)
    autorizados = {}
    for ref, (viejo, nuevo) in cambios().items():
        viejo = entidades.quita(viejo, frec)
        if ref in ANTERIORES:
            viejo = (viejo, ANTERIORES[ref])
        autorizados[ref] = (viejo, entidades.quita(nuevo, frec))
    return autorizados, frec


def quita_restos(entradas, frec):
    """Quita de cada entrada los restos de entidades (entidades.py).

    Devuelve las entradas y las claves cambiadas. Cada cambio solo puede
    quitar el resto y ajustar el espacio de alrededor: si el texto sin
    restos y sin espacios no es el mismo, se detiene (ValueError).
    """
    nuevas, limpias = [], set()
    for clave, cuerpo in entradas:
        limpio = entidades.quita(cuerpo, frec)
        if limpio != cuerpo:
            if entidades.RESTO.search(limpio) or \
                    re.sub(r"\s", "", entidades.RESTO.sub("", cuerpo)) != \
                    re.sub(r"\s", "", limpio):
                raise ValueError(f"{clave}: la limpieza cambió otra cosa")
            limpias.add(clave)
        nuevas.append((clave, limpio))
    return nuevas, limpias


def exporta(sword_path=None, nombre=MODULO):
    env = dict(os.environ)
    if sword_path:
        env["SWORD_PATH"] = sword_path
    r = subprocess.run(["mod2imp", nombre], capture_output=True, text=True,
                       env=env, check=True)
    return r.stdout


INSTALADO = os.path.join(os.path.expanduser("~"), ".sword")


def conf_de(raiz):
    """El .conf del módulo en un árbol SWORD (mods.d/ + modules/)."""
    with open(os.path.join(raiz, "mods.d", "torresamat.conf"),
              encoding="utf-8") as f:
        return f.read()


def exporta_aislado(raiz, conf):
    """Exporta el TorresAmat de `raiz` sin que SWORD mire otro sitio.

    SWORD suma ~/.sword a SWORD_PATH: con el mismo nombre podría leer el
    módulo instalado en lugar del de `raiz`. Se exporta bajo otro nombre,
    con el .conf de `raiz`, y nada de ~/.sword interviene.
    """
    nombre = MODULO + "Aislado"
    with tempfile.TemporaryDirectory() as tmp:
        os.makedirs(os.path.join(tmp, "mods.d"))
        os.symlink(os.path.abspath(os.path.join(raiz, "modules")),
                   os.path.join(tmp, "modules"))
        with open(os.path.join(tmp, "mods.d", "aislado.conf"), "w",
                  encoding="utf-8") as f:
            f.write(conf.replace(f"[{MODULO}]", f"[{nombre}]", 1))
        return exporta(tmp, nombre)


def cambiadas(antes, despues):
    """Claves cuyo texto difiere; exige las mismas claves en el mismo orden."""
    if [c for c, _ in antes] != [c for c, _ in despues]:
        raise ValueError("las claves o su orden no coinciden")
    return [c for (c, a), (_, b) in zip(antes, despues) if a != b]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--origen", default=INSTALADO,
                    help="árbol SWORD de partida (mods.d/ + modules/); por "
                         "defecto el instalado, ~/.sword. Para regenerar "
                         "desde una fuente conocida: modulos/ de git")
    ap.add_argument("--salida", default=os.path.join(DIR, "salida", "parche"))
    args = ap.parse_args()
    if os.path.abspath(args.salida) == os.path.abspath(args.origen):
        sys.exit("--salida no puede ser el --origen")

    conf = conf_de(args.origen)
    original = exporta_aislado(args.origen, conf)
    entradas = lee_imp(original)
    if escribe_imp(entradas) != original:
        sys.exit("el export no se puede reescribir sin pérdida; no se toca")

    autorizados, frec = autoriza(entradas)
    try:
        limpias_entradas, limpias = quita_restos(entradas, frec)
        nuevas = aplica(limpias_entradas, autorizados)
    except ValueError as e:
        sys.exit(str(e))
    parcheado = escribe_imp(nuevas)

    # La salida es un árbol SWORD completo, con el mismo .conf del origen:
    # se puede volver a usar como --origen (la segunda pasada no cambia
    # nada) y copiar tal cual a modulos/ o a ~/.sword, que es otro paso.
    destino = os.path.join(args.salida, "modules", "texts", "ztext",
                           "torresamat")
    shutil.rmtree(args.salida, ignore_errors=True)
    os.makedirs(destino)
    os.makedirs(os.path.join(args.salida, "mods.d"))
    with open(os.path.join(args.salida, "mods.d", "torresamat.conf"), "w",
              encoding="utf-8") as f:
        f.write(conf)
    with tempfile.TemporaryDirectory() as tmp:
        imp = os.path.join(tmp, "torresamat.imp")
        with open(imp, "w", encoding="utf-8") as f:
            f.write(parcheado)
        subprocess.run(["imp2vs", imp, "-z", "z", "-v", V11N, "-o", "."],
                       cwd=destino, check=True, capture_output=True)

    # Ida y vuelta: el módulo nuevo debe exportar exactamente el imp
    # parcheado, y diferir del origen solo en CORRECCIONES/TITULOS.
    vuelta = exporta_aislado(args.salida, conf)
    if vuelta != parcheado:
        sys.exit("la ida y vuelta del módulo parcheado no coincide")
    try:
        distintas = cambiadas(entradas, lee_imp(vuelta))
    except ValueError as e:
        sys.exit(str(e))
    fuera = [c for c in distintas
             if c not in autorizados and c not in limpias]
    if fuera:
        sys.exit(f"cambió algo fuera de CORRECCIONES/TITULOS: {fuera}")
    if limpias:
        print(f"  restos de entidades quitados en {len(limpias)} versos")
    print(f"módulo parcheado en {destino} ({len(distintas)} versos)")


if __name__ == "__main__":
    main()
