"""Columnas fundidas por el OCR (TORRES-PSALM-ALIGN-101).

En la hoja 59 del tomo III el OCR leyó renglón a renglón las dos columnas
de la página: cada verso del Sal 112 lleva detrás uno de «In exitu» (Vulg
113:1-8), y cada verso de «Non nobis» (impreso 1-18, Vulg 113:9-26) lleva
una copia de un verso del 114 o del 115, que ya están en su sitio. El
impreso 6 y 7 de «Non nobis» cayeron juntos en una ranura y el 113:20-26
quedaba vacío. En la hoja 65 pasa lo mismo con el 130/131 y el 132/133, y
el v. 3 del Sal 131 había quedado delante del 5. Sal 58:4 y 77:38 llevaban
el pie de una lámina y la cabecera de la página siguiente; 2 Mac 15:40, el
título del tomo III.

Cada entrada es (texto actual en el módulo, texto del impreso, hoja). El
texto nuevo solo reordena trozos del viejo -- nada se añade --; lo
comprueba parche_facsimil._comprueba_columnas().
"""

COLUMNAS = {
    'Psalms 58:4': (
        'Que ya ves como se han hecho dueños de mi vida: arremeten contra mí hombres de gran fuerza. Ñ A Ú Ú Ed Ma UU o Y ) === MN WU AAA === el ml Jl a = == Lis di pe Mí AJA WN NN ll Ñ W A == I Un de UN 1 Ú NON NN IN CU WE 0) Ñ NU UN MN 7 de BABILONIA DESTRUCCION NN NN AI IN SS NN AN W NM LA 0 W UN i SUENO E AN ¡A e O 41 SALMOS. VU',
        'Que ya ves como se han hecho dueños de mi vida: arremeten contra mí hombres de gran fuerza.',
        'tomo III, hoja 31',
    ),
    'Psalms 77:38': (
        'El Señor empero es misericordioso, y les perdonaba sus pecados, ni acababa del todo con ellos. —Contuvo muchísimas veces su indienacion, y no dió lugar á todo su enojo; EL SEÑOR DA FO mo Sl ¡Ho LR $ 57 SALMOS. E',
        'El Señor empero es misericordioso, y les perdonaba sus pecados, ni acababa del todo con ellos. —Contuvo muchísimas veces su indienacion, y no dió lugar á todo su enojo;',
        'tomo III, hoja 41',
    ),
    'Psalms 112:2': (
        'Sea bendito el Nombre del Señor desde ahora mismo hasta el fin de los siglos. Consagró Dios á su servicio al pueblo de Judá; y estableció su imperio en Israél.',
        'Sea bendito el Nombre del Señor desde ahora mismo hasta el fin de los siglos.',
        'tomo III, hoja 59',
    ),
    'Psalms 112:3': (
        'Desde Oriente hasta Poniente es digno de ser bendecido el Nombre del Señor. El mar le vió, y echó á huir: el Jordan volvió hácia atrás',
        'Desde Oriente hasta Poniente es digno de ser bendecido el Nombre del Señor.',
        'tomo III, hoja 59',
    ),
    'Psalms 112:4': (
        'Excelso es el Señor sobre todas las gentes, y su gloria sobrepuja los cielos. Los montes brincaron de gozo como carneros, y los collados como corderitos.',
        'Excelso es el Señor sobre todas las gentes, y su gloria sobrepuja los cielos.',
        'tomo III, hoja 59',
    ),
    'Psalms 112:5': (
        '¿Quién como el Señor nuestro Dios? El tiene su morada en las alturas, ¿Qué tienes tú, oh mar, que a87 has huido; y tú, oh Jordan, por qué has vuelto atrás?',
        '¿Quién como el Señor nuestro Dios? El tiene su morada en las alturas,',
        'tomo III, hoja 59',
    ),
    'Psalms 112:6': (
        'Y está cuidando de las criaturas humildes en el cielo y en la tierra. Vosotros, oh montes, ¿por qué brincasteis de gozo como carneros; y vosotros, oh collados, como corderitos?',
        'Y está cuidando de las criaturas humildes en el cielo y en la tierra.',
        'tomo III, hoja 59',
    ),
    'Psalms 112:7': (
        'Levanta del polvo de la tierra al desvalido, y alza del estercolero al pobre, Por la presencia del Señor se estremeció la tierra, por la presencia del Dios de Jacob',
        'Levanta del polvo de la tierra al desvalido, y alza del estercolero al pobre,',
        'tomo III, hoja 59',
    ),
    'Psalms 112:8': (
        'Para colocarle entre los príncipes, entre los príncipes de — su pueblo, Que convirtió la peña en estanque de aguas, y en fuentes de aguas la drida roca.',
        'Para colocarle entre los príncipes, entre los príncipes de — su pueblo,',
        'tomo III, hoja 59',
    ),
    'Psalms 112:9': (
        'El ála mujer, antes estéril, la hace vivir en su casa alegre al verse rodeada de hijos. Grandeza de Dios en los prodigios con que libró á su pueblo. Vanidad de los idolos. Aleluya. <chapter eID="gen16248" osisID="Ps.112"/>',
        'El ála mujer, antes estéril, la hace vivir en su casa alegre al verse rodeada de hijos. <chapter eID="gen16248" osisID="Ps.112"/>',
        'tomo III, hoja 59',
    ),
    'Psalms 113:1': (
        'No áÁ NOSOTROS, SEÑOR, No á Nosotros, sino á tu Nombre da toda la gloria, Amé al Señor, seguro de que oirá la voz de mi oracion.',
        '<seg type="x-psalm-title">Aleluya.</seg> Cuando Israél salió de Esypto, al partir la casa de Jacob de en medio de aquel pueblo extranjero',
        'tomo III, hoja 59',
    ),
    'Psalms 113:2': (
        'Para hacer brillar tu misericordia y tu verdad: á fin de que jamás digan los Gentiles: ¿Dónde está su Dios? Porque se dignó inclinar hácia mí sus oidos; y as7 le invocaré en todos los dias de mi vida.',
        'Consagró Dios á su servicio al pueblo de Judá; y estableció su imperio en Israél.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:3': (
        'Nuestro Dios está en los cielos: él ha hecho todo cuanto quiso.',
        'El mar le vió, y echó á huir: el Jordan volvió hácia atrás',
        'tomo III, hoja 59',
    ),
    'Psalms 113:4': (
        'Los idolos de las naciones l no son mas que plata y oro, obra de las manos de los hombres. É invoqué el Nombre del Señor.—Libra, oh Señor, el alma mia',
        'Los montes brincaron de gozo como carneros, y los collados como corderitos.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:5': (
        'Boca tienen, mas no hablarán; tienen ojos, pero jamás? verán. y Cercáronme mortales angustias, me embistieron los horrores del infierno, ó sepulero.—Me hallé en medio de la tribulacion y del dolor; Misericordioso es el Señor, y justo: compasivo es nuestro Dios.',
        '¿Qué tienes tú, oh mar, que a87 has huido; y tú, oh Jordan, por qué has vuelto atrás?',
        'tomo III, hoja 59',
    ),
    'Psalms 113:6': (
        'Orejas E tienen, y nada oirán; narices, y no olerán. 7 Tienen manos, y ho palparán; piés, mas no andarán; ni articularán una voz econ su garganta. El Señor guarda á los pequeñuelos: yo me humillé, y él me saco á paz y á salvo.',
        'Vosotros, oh montes, ¿por qué brincasteis de gozo como carneros; y vosotros, oh collados, como corderitos?',
        'tomo III, hoja 59',
    ),
    'Psalms 113:7': (
        'Vuelve, oh alma mia, á tu sosiego; ya que el Señor te ha favorecido tanto. l',
        'Por la presencia del Señor se estremeció la tierra, por la presencia del Dios de Jacob',
        'tomo III, hoja 59',
    ),
    'Psalms 113:8': (
        'Semejantes sean á estos ídolos los que los hacen, y cuantos ponen en ellos su confianza. Pues él ha librado de la muerte á mi alma, ha enjugado mis lágrimas, y apartado mis piés del precipicio.',
        'Que convirtió la peña en estanque de aguas, y en fuentes de aguas la drida roca.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:9': (
        'La casa de Israél colocó en el Señor su esperanza; el neñor es su amparo y su proteccion, Acepto seré yo al Señor en la region de los vivos. SALMO OXV Accion de gracias á Dios por sus beneficios. Aleluya.',
        'No áÁ NOSOTROS, SEÑOR, No á Nosotros, sino á tu Nombre da toda la gloria,',
        'tomo III, hoja 59',
    ),
    'Psalms 113:10': (
        'La casa de Aaron esperó en el Señor; el Señor es su amparo y su proteccion. Creí d Dios; por eso hablé contado 5, aunque me ví reducido al mayor abatimiento, 85 SALMOS.',
        'Para hacer brillar tu misericordia y tu verdad: á fin de que jamás digan los Gentiles: ¿Dónde está su Dios?',
        'tomo III, hoja 59',
    ),
    'Psalms 113:11': (
        'En el Señor han esperado los que le temen y adoran |: el Señor es su amparo y su proteccion. Yo dije en mi trasporte de ánimo, ó perturbacion: Todos los hombres son falaces.',
        'Nuestro Dios está en los cielos: él ha hecho todo cuanto quiso.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:12': (
        'Acordóse de nosotros el Señor, y nos bendijo. —Bendijo á la casa de Israel, bendijo á la casa de Aaron. Mas¿cómo podré corresponder al Señor por todas las mercedes que me ha hecho?',
        'Los idolos de las naciones l no son mas que plata y oro, obra de las manos de los hombres.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:13': (
        'Bendijo a á todos los que temen al Señor, - así, á los pequeños, como á los grandes. Tomaré el cáliz? de la salud, éinvocaré el Nombre del Señor.',
        'Boca tienen, mas no hablarán; tienen ojos, pero jamás? verán.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:14': (
        'Anmente el Señor sobre vosotros sus bendiciones, sobre vosotros y sobre vuestros hijos. Cumpliré al Señor mis votos en presencia de todo su pueblo.',
        'Orejas E tienen, y nada oirán; narices, y no olerán.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:15': (
        'benditos seais vosotros del Señor, el cual hizo el cielo y la tierra. De gran precio es á los ojos del Señor la muerte de sus santos.',
        'Tienen manos, y ho palparán; piés, mas no andarán; ni articularán una voz econ su garganta.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:16': (
        'El cielo empíreo es para el Señor; mas la tierra la dió á los hijos de los hombres, Oh Señor, siervo tuyo soy, siervo tuyo, ée hijo de esclava tuya. —Tú rompiste mis cadenas',
        'Semejantes sean á estos ídolos los que los hacen, y cuantos ponen en ellos su confianza.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:17': (
        'Oh Señor, no te alabarán los muertos, ni cuantos descienden al sepulero. A tí ofreceré yo un sacrificio de alabanza, é inyocaré el Nombre del Señor.',
        'La casa de Israél colocó en el Señor su esperanza; el neñor es su amparo y su proteccion,',
        'tomo III, hoja 59',
    ),
    'Psalms 113:18': (
        'Nosotros sí, los que vivimos, bendecimos al Señor desde ahora, y por todos los siglos. SALMO OXIV Accion de gracias á Dios por su auxilio en un grande peligro. Aleluya, Cumpliré mis votos al Señor á vista de todo su pueblo',
        'La casa de Aaron esperó en el Señor; el Señor es su amparo y su proteccion.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:19': (
        'En los atrios de la Casa del Señor, en medio de tí, oh Jerusalen. SALMO OXVI Judios y Gentiles deben alabar á Dios por haberles dado el Mesías prometido. Aleluya. <chapter eID="gen16258" osisID="Ps.113"/>',
        'En el Señor han esperado los que le temen y adoran |: el Señor es su amparo y su proteccion.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:20': (
        '',
        'Acordóse de nosotros el Señor, y nos bendijo. —Bendijo á la casa de Israel, bendijo á la casa de Aaron.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:21': (
        '',
        'Bendijo a á todos los que temen al Señor, - así, á los pequeños, como á los grandes.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:22': (
        '',
        'Anmente el Señor sobre vosotros sus bendiciones, sobre vosotros y sobre vuestros hijos.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:23': (
        '',
        'benditos seais vosotros del Señor, el cual hizo el cielo y la tierra.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:24': (
        '',
        'El cielo empíreo es para el Señor; mas la tierra la dió á los hijos de los hombres,',
        'tomo III, hoja 59',
    ),
    'Psalms 113:25': (
        '',
        'Oh Señor, no te alabarán los muertos, ni cuantos descienden al sepulero.',
        'tomo III, hoja 59',
    ),
    'Psalms 113:26': (
        '',
        'Nosotros sí, los que vivimos, bendecimos al Señor desde ahora, y por todos los siglos. <chapter eID="gen16258" osisID="Ps.113"/>',
        'tomo III, hoja 59',
    ),
    'Psalms 114:9': (
        'Acepto seré yo al Señor en la region de los vivos. SALMO OXV Accion de gracias á Dios por sus beneficios. Aleluya. Creí d Dios; por eso hablé contado 5, aunque me ví reducido al mayor abatimiento <chapter eID="gen16278" osisID="Ps.114"/>',
        'Acepto seré yo al Señor en la region de los vivos. <chapter eID="gen16278" osisID="Ps.114"/>',
        'tomo III, hoja 59',
    ),
    'Psalms 115:10': (
        'En los atrios de la Casa del Señor, en medio de tí, oh Jerusalen. SALMO OXVI Judios y Gentiles deben alabar á Dios por haberles dado el Mesías prometido. Aleluya. Alabad al Señor, naciones todas de la tierra: pueblos todos cantad sus alabanzas. Porque su misericordia se ha confirmado sobre nosotros; y la verdad del Señor permanece eternamente. <chapter eID="gen16288" osisID="Ps.115"/>',
        'En los atrios de la Casa del Señor, en medio de tí, oh Jerusalen.',
        'tomo III, hoja 59',
    ),
    'Psalms 131:2': (
        'Bi yo no he sentido bajamente de mí, sino que al contrario se ha ensoberbecido mi ánimo; como el niño recien destetado está penaundo en los brazos de su madre, tal sea la pena dentro de mi corazon. De cómo juró al Señor, é hizo voto al Dios de Jacob, diciendo',
        'De cómo juró al Señor, é hizo voto al Dios de Jacob, diciendo',
        'tomo III, hoja 65',
    ),
    'Psalms 131:3': (
        'Espere Israél en el Señor, desde ahora y por siempre jamás. SALMO CONXNXXI Ruega el pueblo á Dios que restaure su remo por medio del Mestas',
        'No me meteré yo al abrigo de mi casa: no subiré á reposar en mi lecho:',
        'tomo III, hoja 65',
    ),
    'Psalms 131:5': (
        'No me meteré yo al abrigo de mi casa: no subiré á reposar en mi lecho: Nireclinaré mis sienes hasta que tenga una habitacion para el Señor, un tabernáculo para el Dios de Jacob.',
        'Nireclinaré mis sienes hasta que tenga una habitacion para el Señor, un tabernáculo para el Dios de Jacob.',
        'tomo III, hoja 65',
    ),
    'Psalms 132:3': (
        'Como el rocío que cae sobre el monte Hermon, como el que desciende sobre el monte Sion, — Pues allí donde reina la concordia, derrama el Señor sus bendiciones y vida sempiterna, SALMO OXXXIII Exhortacion á los ministros del Señor para que le alaben Cántico gracual. Ea pues, bendecid al Señor ahora mismo, vosotros todos, oh siervos del Señor. Vosotros los que asistís en la Casa del Señor, en los atrios del templo de nuestro Dios, Levantad por las noches vuestras manos hácia el Santuario, y alabad al Señor, Bendígate desde Sion el Señor que crió el cielo y la tierra <chapter eID="gen16602" osisID="Ps.132"/>',
        'Como el rocío que cae sobre el monte Hermon, como el que desciende sobre el monte Sion, — Pues allí donde reina la concordia, derrama el Señor sus bendiciones y vida sempiterna.',
        'tomo III, hoja 65',
    ),
    'Psalms 133:2': (
        'Es como el o/oroso perfume, que derramado en la cabeza, va destilando por la respetable barba de Aaron,—y desciende hasta la orla de su vestidura: Levantad por las noches vuestras manos hácia el Santuario, y alabad al Señor',
        'Levantad por las noches vuestras manos hácia el Santuario, y alabad al Señor',
        'tomo III, hoja 65',
    ),
    'Psalms 133:3': (
        'Como el rocío que cae sobre el monte Hermon, como el que desciende sobre el monte Sion, — Pues allí donde reina la concordia, derrama el Señor sus bendiciones y vida sempiterna, SALMO OXXXIII Exhortacion á los ministros del Señor para que le alaben Bendígate desde Sion el Señor que crió el cielo y la tierra <chapter eID="gen16606" osisID="Ps.133"/>',
        'Bendígate desde Sion el Señor que crió el cielo y la tierra <chapter eID="gen16606" osisID="Ps.133"/>',
        'tomo III, hoja 65',
    ),
    'II Maccabees 15:40': (
        'Pues así como es cosa dañosa el beber siempre vino, ó siempre agua, al paso que es grato el usar ora de uno, ora de otro: así tambien un discurso gustaria poco á los lectores, si el estilo fuese siempre muy peinado y uniforme. Y con esto doy fin. TOMO III LIBRO DE LOS SALMOS. <chapter eID="gen27429" osisID="2Macc.15"/> <div eID="gen26912" osisID="2Macc" type="book"/>',
        'Pues así como es cosa dañosa el beber siempre vino, ó siempre agua, al paso que es grato el usar ora de uno, ora de otro: así tambien un discurso gustaria poco á los lectores, si el estilo fuese siempre muy peinado y uniforme. Y con esto doy fin. <chapter eID="gen27429" osisID="2Macc.15"/> <div eID="gen26912" osisID="2Macc" type="book"/>',
        'tomo III, portada',
    ),
}
