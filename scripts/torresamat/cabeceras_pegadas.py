"""Encabezados de salmo pegados al versículo anterior (TORRES-PSALM-GLUE-101).

El OCR juntó al último versículo de un salmo lo que el impreso compone
entre ese versículo y el «1.» del siguiente: el encabezado «SALMO …», el
argumento en cursiva y a veces el comienzo del título, que ya está en su
versículo 1 (TITULOS). En Sal 92:5, 146:11 y 147:9 arrastró además el
salmo siguiente entero, que ya está en sus versículos. Nada de eso es texto
del versículo: se quita. Sal 18:15 y 117:14 llevan en medio la cabecera
de página («SALMO XX. 14», «SALMO CXVIII. 86»), ruido del grabado y una
nota al pie.

Cada entrada es (texto actual en el módulo, texto del impreso, hoja del
tomo III); cada caso se ha cotejado con esa hoja del facsímil. Lo que se
añade está en el impreso y el OCR lo perdió (AÑADIDOS); el punto final
sustituye a la coma que el OCR leyó en la llamada de nota o el punto.
Sal 117:15 estaba vacío: su texto es el que el OCR pegó a 117:14
(TRASLADADOS). Sal 147:2 repetía el 147:3 (REPETIDOS).
"""

CABECERAS = {
    'Psalms 6:11': (
        'Avergiiéncense, y queden llenos de la mayor turbacion todos mis enemigos: retírense, y váyanse al momento cubiertos de ignominia. SALMO VIH Implora la justicia del Señor, para que le defienda de sus enemigos; cuya ruina predice <chapter eID="gen14363" osisID="Ps.6"/>',
        'Avergiiéncense, y queden llenos de la mayor turbacion todos mis enemigos: retírense, y váyanse al momento cubiertos de ignominia. <chapter eID="gen14363" osisID="Ps.6"/>',
        'tomo III, hoja 12',
    ),
    'Psalms 18:15': (
        'Con lo que te serán aceptas las palabras ó cánticos de mi boca, como tambien la meditacion de mi corazon que 3 er Ea, a, P al k SALMO XX. 14 hare yo siempre en tu acatamiento.—Oh Señor, amparo mio, y Redentor mio. <chapter eID="gen14538" osisID="Ps.18"/>',
        'Con lo que te serán aceptas las palabras ó cánticos de mi boca, como tambien la meditacion de mi corazon que hare yo siempre en tu acatamiento.—Oh Señor, amparo mio, y Redentor mio. <chapter eID="gen14538" osisID="Ps.18"/>',
        'tomo III, hoja 16',
    ),
    'Psalms 21:32': (
        'Será contada como del Señor la generacion venidera: y los cielos? anunciarán la justicia de él al pueblo que ha de nacer, formado por el Señor. O SALMO XXIMT A Á quien Dios apacienta nada le falta. <chapter eID="gen14579" osisID="Ps.21"/>',
        'Será contada como del Señor la generacion venidera: y los cielos? anunciarán la justicia de él al pueblo que ha de nacer, formado por el Señor. <chapter eID="gen14579" osisID="Ps.21"/>',
        'tomo III, hoja 16',
    ),
    'Psalms 40:14': (
        'Bendito sea el Señor Dios de Israél por los siglos de los siglos: ¡; Así sea! ¡Así sen! SALMO XLI Durid en medio de las tribulaciones se consuela con la memoria de los bienes celestiales, y la esperonza de su libertad. Para el fin <chapter eID="gen14927" osisID="Ps.40"/>',
        'Bendito sea el Señor Dios de Israél por los siglos de los siglos: ¡; Así sea! ¡Así sen! <chapter eID="gen14927" osisID="Ps.40"/>',
        'tomo III, hoja 26',
    ),
    'Psalms 47:15': (
        'Porque aquí está Dios, el Dios nuestro, para siempre y por los sielos de los siglos: él nos yobernará eternamente, SALMO XLVIIMT Exhortación e la viriud y á la fuga del vicio. <chapter eID="gen15029" osisID="Ps.47"/>',
        'Porque aquí está Dios, el Dios nuestro, para siempre y por los sielos de los siglos: él nos yobernará eternamente. <chapter eID="gen15029" osisID="Ps.47"/>',
        'tomo III, hoja 28',
    ),
    'Psalms 57:12': (
        'Entonces dirán los hombres: Pues que el justo recibe su galardon, es indudable que hay un Dios que ejerce su juicio sobre ellos en la tierra. SALMO EVIL Duvid, puesto en grande riesgo de caer en manos de Saul, recurre d DIOS, y se salva por la oración. Se ve figurado el castigo de los Judios por no reconocer al Mestas. Para el fin <chapter eID="gen15188" osisID="Ps.57"/>',
        'Entonces dirán los hombres: Pues que el justo recibe su galardon, es indudable que hay un Dios que ejerce su juicio sobre ellos en la tierra. <chapter eID="gen15188" osisID="Ps.57"/>',
        'tomo III, hoja 31',
    ),
    'Psalms 67:36': (
        'Admirable es Dios en sus santos, ó en su Santuario": el Dios de Israél, él mismo dará virtud y fortaleza á su pueblo. Bendito sea Dios, SALMO EXVIIT Dolores acerbisimos del Redentor en su pasion: castigo de sus perseguwidores: y fundación de la Iglesia sobre las ruimas de la Synagoga. <chapter eID="gen15318" osisID="Ps.67"/>',
        'Admirable es Dios en sus santos, ó en su Santuario": el Dios de Israél, él mismo dará virtud y fortaleza á su pueblo. Bendito sea Dios. <chapter eID="gen15318" osisID="Ps.67"/>',
        'tomo III, hoja 37',
    ),
    'Psalms 78:13': (
        'Entre tanto nosotros, pueblo tuyo y ovejas de tu grey, cantaremos perpétuamente tus alabanzas: de generacion en generacion publicaremos tus glorias. SALMO ELXXIX Prediccion de la cautividad del pueblo de Israel entre los Assyrios, y de su libertad; figura de la esclavitud del género humano bajo el poder del demonto, y de la redención de Christo. <chapter eID="gen15607" osisID="Ps.78"/>',
        'Entre tanto nosotros, pueblo tuyo y ovejas de tu grey, cantaremos perpétuamente tus alabanzas: de generacion en generacion publicaremos tus glorias. <chapter eID="gen15607" osisID="Ps.78"/>',
        'tomo III, hoja 44',
    ),
    'Psalms 82:19': (
        'Y conozcan que te es propio el nombre de SEÑOR, ó de Jehovah, y que solo tú eres el Altísimo en toda la tierra. SALMO LXXXUHI Expresa el Profeta sus ardientes ansias de habitar en el Tabernáculo de Dios, de que está alejado. <chapter eID="gen15668" osisID="Ps.82"/>',
        'Y conozcan que te es propio el nombre de SEÑOR, ó de Jehovah, y que solo tú eres el Altísimo en toda la tierra. <chapter eID="gen15668" osisID="Ps.82"/>',
        'tomo III, hoja 46',
    ),
    'Psalms 83:13': (
        'No dejará sin bienes á los que proceden con inocencia. Oh Señor de los ejércitos, bienaventurado el hombre que pone o SALMO LXXXIV Ruega el Salmista á Dios que se muestre propicio á aquellos que ha libra¡ y do de la esclavitud. Habla con tanta seguridad de la promesa del Mestas, como sí la viese ya cumplida. <chapter eID="gen15687" osisID="Ps.83"/>',
        'No dejará sin bienes á los que proceden con inocencia. Oh Señor de los ejércitos, bienaventurado el hombre que pone en tí su esperanza. <chapter eID="gen15687" osisID="Ps.83"/>',
        'tomo III, hoja 46',
    ),
    'Psalms 92:5': (
        'Tus testimonios se han hecho por extremo crelbles. La santidad debe ser, Señor, el ornamento de tu Casa por la série de los siglos. SALMO XCHI De la justicia, y providencia de Dios en el castigo de los malos, y en el premio de los buenos. Salmo del mismo David, para el cuarto dia de la semana, L. El Señor ó Jehovah, es el Dios de las venganzas: y el Dios de las venganzas ha obrado con independiente libertad. <chapter eID="gen15862" osisID="Ps.92"/>',
        'Tus testimonios se han hecho por extremo crelbles. La santidad debe ser, Señor, el ornamento de tu Casa por la série de los siglos. <chapter eID="gen15862" osisID="Ps.92"/>',
        'tomo III, hoja 49',
    ),
    'Psalms 95:13': (
        'Á la vista del Señor, porque viene: viene, sí, á gobernar la tierra. —Gobernará la redondez de la tierra con justicia: gobernará á los pueblos con su verdad, SALMO XOVI Profetiza David el establecimiento del reino espiritual de Jesu-Christo, y exhorta G los hombres ú prepararse para entrar en el, Puede tambien entenderse este Salmo de la segunda venida de Jesu-Christo al mundo. <chapter eID="gen15901" osisID="Ps.95"/>',
        'Á la vista del Señor, porque viene: viene, sí, á gobernar la tierra. —Gobernará la redondez de la tierra con justicia: gobernará á los pueblos con su verdad. <chapter eID="gen15901" osisID="Ps.95"/>',
        'tomo III, hoja 50',
    ),
    'Psalms 96:12': (
        'Alegraos pues, oh justos, en el Señor; y celebrad con alabanzas su santa memoria, SALMO XOVII Sigue el mismo argumento del Salmo precedente <chapter eID="gen15914" osisID="Ps.96"/>',
        'Alegraos pues, oh justos, en el Señor; y celebrad con alabanzas su santa memoria. <chapter eID="gen15914" osisID="Ps.96"/>',
        'tomo III, hoja 50',
    ),
    'Psalms 97:9': (
        'Á la vista del Señor: porque viene á gobernar! la tierra, —El juzgará el orbe terráqueo con justicia, y á los pueblos con rectitud. SALMO XCVIHIHI Celebra el Salmista el reino de Dios y de su Christo; y convida ú todos los hombres ú reconocer ú este Dios supremo, d quien sirvieron Moysés, daron y demás profetas. <chapter eID="gen15927" osisID="Ps.97"/>',
        'Á la vista del Señor: porque viene á gobernar! la tierra, —El juzgará el orbe terráqueo con justicia, y á los pueblos con rectitud. <chapter eID="gen15927" osisID="Ps.97"/>',
        'tomo III, hoja 51',
    ),
    'Psalms 99:5': (
        'Porque es un Señor lleno de bondad: es eterna su misericordia; y su verdad resplandecerá de generacion en generacion, SALMO e Fietrato de un rey pio y justo, en que deben mirarse los principes para el gobierno de sus Estados. <chapter eID="gen15946" osisID="Ps.99"/>',
        'Porque es un Señor lleno de bondad: es eterna su misericordia; y su verdad resplandecerá de generacion en generacion. <chapter eID="gen15946" osisID="Ps.99"/>',
        'tomo III, hoja 51',
    ),
    'Psalms 101:29': (
        'Los hijos de tus siervos habitarán tranquilos en Jeyrusalem, y su descendencia quedará arraigada por los siglos de los siglos. SALMO CH Accion de gracias á Dios por la remisión de los pecados y demás inmensos beneficios que de él recibimos, as <chapter eID="gen15961" osisID="Ps.101"/>',
        'Los hijos de tus siervos habitarán tranquilos en Jeyrusalem, y su descendencia quedará arraigada por los siglos de los siglos. <chapter eID="gen15961" osisID="Ps.101"/>',
        'tomo III, hoja 52',
    ),
    'Psalms 109:7': (
        'Beberá del torrente durante el camino: por eso levantará su cabeza, SALMO OX Los obras del Señor son admirables, así en el Antiguo como en el Nuevo Testamento. Aleluya. <chapter eID="gen16219" osisID="Ps.109"/>',
        'Beberá del torrente durante el camino: por eso levantará su cabeza. <chapter eID="gen16219" osisID="Ps.109"/>',
        'tomo III, hoja 58',
    ),
    'Psalms 117:14': (
        'El Señor es mi fortaleza y mi gloria; el Señor se ha constituido salvacion mia.: 1 Prescrito por la Ley para dar gracias á Dios.—Véase Cáliz, — SALMO COXVILL. 386 a) 11) 15. Voces de júbilo y de salvacion son las que se oyen en las moradas de los justos.',
        'El Señor es mi fortaleza y mi gloria; el Señor se ha constituido salvacion mia.',
        'tomo III, hoja 60',
    ),
    'Psalms 117:15': (
        '',
        'Voces de júbilo y de salvacion son las que se oyen en las moradas de los justos.',
        'tomo III, hoja 60',
    ),
    'Psalms 117:29': (
        'Alabad al Señor por ser imfinitamente bueno; por ser eterna su misericordia. SALMO OXVIHI? Encomios de la Ley de Dios: oracion para pedir á Dios la gracia de entenderla, amarla, y observarla. ot Aleluya. <chapter eID="gen16302" osisID="Ps.117"/>',
        'Alabad al Señor por ser imfinitamente bueno; por ser eterna su misericordia. <chapter eID="gen16302" osisID="Ps.117"/>',
        'tomo III, hoja 60',
    ),
    'Psalms 120:8': (
        'El Señor te guardará en todos los pasos de tu vida, desde ahora y para siempre. SALMO CXXI Bajo la alegoría de los que iban á visitar el templo del Señor en Jerusalem en las tres fiestas solemnes del año, y publicaban las excelencias de aquella ciudad santa, sé representan las alabanzas de la Iglesia de Jesu-Uhristo y de la celestial Jerusalem. <chapter eID="gen16507" osisID="Ps.120"/>',
        'El Señor te guardará en todos los pasos de tu vida, desde ahora y para siempre. <chapter eID="gen16507" osisID="Ps.120"/>',
        'tomo III, hoja 63',
    ),
    'Psalms 127:6': (
        'Y veas á los hijos de tus hijos, y la paz en Israél, SALMO OXXVIII El Profeta exhorta a los hijos de Esrae! Ed alabar al Señor por la proteccion que les ha concedido <chapter eID="gen16556" osisID="Ps.127"/>',
        'Y veas á los hijos de tus hijos, y la paz en Israél. <chapter eID="gen16556" osisID="Ps.127"/>',
        'tomo III, hoja 64',
    ),
    'Psalms 128:8': (
        'Ni dijeron los pasajeros: La bendicion del Señor continúe sobre vosotros: os la deseamos en el Nombre del Señor. SALMO OXXIX El verdadero penitente confiesa sus pecados, y espera el perdon de la misericordia de Dios. <chapter eID="gen16563" osisID="Ps.128"/>',
        'Ni dijeron los pasajeros: La bendicion del Señor continúe sobre vosotros: os la deseamos en el Nombre del Señor. <chapter eID="gen16563" osisID="Ps.128"/>',
        'tomo III, hoja 65',
    ),
    'Psalms 131:18': (
        'Á sus enemigos los cubriré de oprobio; mas en él brillará la gloria de mi propia santidad. SALMO OXXXI Compárase el placer que causan la consordia y caridad fraternal, con la fragancia del bálsamo precioso. <chapter eID="gen16583" osisID="Ps.131"/>',
        'Á sus enemigos los cubriré de oprobio; mas en él brillará la gloria de mi propia santidad. <chapter eID="gen16583" osisID="Ps.131"/>',
        'tomo III, hoja 65',
    ),
    'Psalms 139:14': (
        'Y así los justos glorificarán eternamente tu santo Nombre, y los hombres de probidad gozarán de la vista de tu divtSALMO CXEL Pide ú Dios la paciencia en las tribulaciones, y que le defienda de sus ENEMITOS. <chapter eID="gen16695" osisID="Ps.139"/>',
        'Y así los justos glorificarán eternamente tu santo Nombre, y los hombres de probidad gozarán de la vista de tu divina cara. <chapter eID="gen16695" osisID="Ps.139"/>',
        'tomo III, hoja 67',
    ),
    'Psalms 141:8': (
        'Saca de esta cárcel á mi alma para que alabe tu santo Nombre: esperando están los justos el momento en que me seas propicio. SALMO CXELIH Tmplora David el socorro del Señor, y le pide perseverancia en la nueva vida Castiza Dios d Sus enemigos Salmo de David <chapter eID="gen16720" osisID="Ps.141"/>',
        'Saca de esta cárcel á mi alma para que alabe tu santo Nombre: esperando están los justos el momento en que me seas propicio. <chapter eID="gen16720" osisID="Ps.141"/>',
        'tomo III, hoja 68',
    ),
    'Psalms 143:15': (
        'Feliz llamaron al pueblo que goza de estas cosas. Ius yo digo: Feliz aquel pueblo que tiene al Señor por su Dios, SALMO OXLIV Aluba á Dios, que como rey bueno y misericordioso gobierna y conserva Joxdas las cosas Conviene d Jesu-Christo <chapter eID="gen16741" osisID="Ps.143"/>',
        'Feliz llamaron al pueblo que goza de estas cosas. Ius yo digo: Feliz aquel pueblo que tiene al Señor por su Dios. <chapter eID="gen16741" osisID="Ps.143"/>',
        'tomo III, hoja 69',
    ),
    'Psalms 146:11': (
        'Se complace sí en aquellos que le temen y adoran, y en los que confian en su misericordia. SALMO CXLVIL Debemos alabar al Señor, porque solo él es el que nos de todos los bienes; y es Jerusalem una ciudad especialmente favorecida de Dios. e: Aleluya. Alaba al Señor, oh Jerusalem; alaba, oh Sion, á tu Porque él ha asegurado con fuertes barras 9 cerrojos tus puertas; ha llenado de bendicion úá tus hijos, que moran dentro de tí. me Y l4. Ha establecido la paz en tu terr itorio, y te alimenta de la flor de harina. DESPUES DE SU RUINA a UNO El despacha sus órdenes á la tierra; órdenes que se comunican velocisimamente, Él mos da la nieve como copos de lana: esparce la escarcha como ceniza, Él despide el granizo en menudos pedazos: al rigor de su frio ¿quién resistirá? Pero ego despacha sus órdenes, y derrite estas cosas: hace soplar su viento, y finyen las aguas. El anuncia su palabra á Jacob, sus preceptos y ocultos juicios á Israél. No ha hecho otro tanto con las demás naciones: ni les ha manifestado á ellas sus juicios ó preceptos. Aleluya. SALMO CXEVIHI Jl Profeta convida á todas las eriaturas á alabar á su Criador. <chapter eID="gen16785" osisID="Ps.146"/>',
        'Se complace sí en aquellos que le temen y adoran, y en los que confian en su misericordia. <chapter eID="gen16785" osisID="Ps.146"/>',
        'tomo III, hoja 69',
    ),
    'Psalms 147:2': (
        'Porque él ha asegurado con fuertes barras 9 cerrojos tus puertas; ha llenado de bendicion úá tus hijos, que moran dentro de tí. me Y l4. Ha establecido la paz en tu terr itorio, y te alimenta de la flor de harina.',
        'Porque él ha asegurado con fuertes barras 9 cerrojos tus puertas; ha llenado de bendicion úá tus hijos, que moran dentro de tí.',
        'tomo III, hoja 69',
    ),
    'Psalms 147:9': (
        'No ha hecho otro tanto con las demás naciones: ni les ha manifestado á ellas sus juicios ó preceptos. Aleluya. SALMO CXEVIHI Jl Profeta convida á todas las eriaturas á alabar á su Criador. Alehiya. Alabad al Señor vosotros que estais en los cielos; alabadle los que estais en las alturas, Alabadle todos vosotros, ángeles suyos; alabadle vosotras todas, milicias suyas. Alabadle, oh sol y luna: alabadle todas vosotras, Iueientes estrellas. Alábale tú, oh cielo empíreo, y alaben el Nombre del Senor todas las aguas que están sobre el firmamento. Porque el Señor habló, y con solo quererlo, quedaron hechas las cosas: él mandó que existiesen, y quedaron criadas. Estableciólas para que subsistiesen eternamente y por todos los siglos: fijóles un órden que observarán siempre. Alabad al Señor vosotras criaturas de la tierra; mónstruos del mar, y vosotros todos, oh abismos. Fuego, granizo, nieve, hielo, vientos procelosos, vosotros que ejecutais sus órdenes: Montes y collados todos, plantas fructíferas, y todos vosotros, oh cedros: bestias todas silvestres y domésticas, reptiles y volátiles; Reyes de la tierra y pueblos todos; principes y jueces todos de la tierra: Los jóvenes y las vírgenes, los ancianos y los niños, todas las criaturas canten alabanzas al Nombre del Senor; Porque solo el Nombre del Senor, yy no otro, es digno de ser ensalzado. Su gloria resplandece sobre cielos y tierra; y él es el <chapter eID="gen16796" osisID="Ps.147"/>',
        'No ha hecho otro tanto con las demás naciones: ni les ha manifestado á ellas sus juicios ó preceptos. Aleluya. <chapter eID="gen16796" osisID="Ps.147"/>',
        'tomo III, hoja 72',
    ),
}

# Lo que el OCR perdió y el impreso trae (ver la hoja de cada entrada).
AÑADIDOS = {
    # «oh Sion, á tu Dios.»: se completa en TITULOS, que ya marca el verso.
    'Psalms 147:1': ' Dios.',
    'Psalms 83:13': ' en tí su esperanza.',
    # «divi-/na cara.»: el OCR leyó «divt» y perdió el renglón.
    'Psalms 139:14': 'divina cara.',
}

# Nota al pie que el OCR metió entre el versículo y la cabecera de página.
NOTAS = {'Psalms 117:14': '1 Prescrito por la Ley para dar gracias á Dios.'}

# Versículo vacío -> versículo cuya cola trae su texto.
TRASLADADOS = {'Psalms 117:15': 'Psalms 117:14'}

# Versículo -> el siguiente, que ya trae lo que se le quita.
REPETIDOS = {'Psalms 147:2': 'Psalms 147:3'}
