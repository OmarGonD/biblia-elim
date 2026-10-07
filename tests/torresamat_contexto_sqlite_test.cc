#include "backend/sqlite/sqlite_bible_backend.h"

#include <cstdio>
#include <string>

int main(int argc, char **argv)
{
	SqliteBibleBackend backend(argc == 2 ? argv[1] : SRCDIR "/modulos/sqlite");
	const std::string module = "TorresAmat";
	int failures = 0;
	const auto check = [&](bool ok, const char *message) {
		if (!ok) { ++failures; std::fprintf(stderr, "%s\n", message); }
	};
	check(backend.hasModule(module), "TorresAmat SQLite module missing");
	check(backend.versification(module) == "Vulg", "native versification changed");
	const struct { const char *reference, *fragment; } cases[] = {
		{"Mark 1:15", "Y diciendo: se ha cumplido ya el tiempo, y el reino de Dios está cerca: haced penitencia, y creed al Evangelio."},
		{"Mark 1:39", "Iba pues Jesus predicando en sus synagogas, y por toda la Galiléa, y expelia los demonios."},
		{"Mark 1:45", "Mas aquel hombre, así que se fué, comenzó á hablar de su curacion, y á publicarla por todas partes, de modo que ya no podia Jesus entrar manifiestamente en la ciudad, sino que andaba fuera por lugares solitarios, y acudian á él de todas partes"},
		{"Mark 2:17", "Habiéndolo oido Jesus les dijo: Los que están buenos no necesitan de médico, sino los que están enfermos: así yo no he venido á llamar á convertir á los justos, sino á los pecadores."},
		{"Gen 2:7", "un soplo ó espíritu de vida"},
		{"John 18:18", "habian ido á prender á Jesus"},
		{"Num 28:14", "Las libaciones ú ofrendas de vino"},
		{"Josh 7:8", "delante de sus enemigos"},
		{"1Kgs 8:26", "Sí, oh Señor Dios de Israél"},
		{"Ps 36:4", "cuanto desea tu corazon"},
		{"Ezek 24:12", "Se ha trabajado con afan"},
		{"John 4:45", "de los Galiléos"},
		{"Bar 5:6", "por los enemigos"},
		{"Exod 28:4", "la otra interior de lino ajustada"},
		{"Job 7:15", "muerte para mis huesos"},
		{"Job 13:17", "enigmas que voy á deciros."},
		{"Sir 18:19", "Antes del juicio ó de presentarte al juez"},
		{"Sir 29:34", "mi casa: ó he de alojar"},
		{"1Macc 4:48", "casa ó templo, y santificaron"},
		{"Deut 20:4", "Porque el Señor Dios vuestro está en medio de vosotros y peleará por vosotros contra los enemigos para libraros del peligro."},
		{"Ps 119:3", "¿Qué se te dará, ó qué fruto sacarás de tus calumnias, oh lengua fraudulenta?"},
		{"Prov 16:3", "Dirige hácia el Señor tus obras; y tendrán buen éxito tus designios."},
		{"Acts 27:38", "Estando ya satisfechos, aligeraban la nave, arrojando al mar el trigo."},
	};
	for (const auto &c : cases) {
		BibleKeyInfo key;
		check(backend.resolveKey(module, c.reference, key), c.reference);
		const auto content = backend.getVerseContent(module, key.reference);
		check(content.valid && content.plainText.find(c.fragment) != std::string::npos, c.reference);
		if (std::string(c.reference) == "Mark 1:15" || std::string(c.reference) == "Mark 1:39" || std::string(c.reference) == "Mark 1:45" || std::string(c.reference) == "Mark 2:17") {
			check(content.plainText == c.fragment, c.reference);
			const auto chapter = backend.getChapter(module, key.reference, false);
			bool found = false;
			for (const auto &verse : chapter)
				if (verse.reference.verse == key.reference.verse && verse.text == c.fragment) found = true;
			check(found, c.reference);
		}
	}
	BibleSearchQuery query;
	query.mode = BibleSearchMode::Indexed;
	query.text = "\"se ha cumplido\"";
	bool found = false;
	for (const auto &result : backend.search(module, query))
		if (result.osisRef == "Mark.1.15" && result.text.find("se ha cumplido") != std::string::npos)
			found = true;
	check(found, "FTS search did not find corrected Mark 1:15");
	query.text = "\"Iba pues Jesus\"";
	found = false;
	for (const auto &result : backend.search(module, query))
		if (result.osisRef == "Mark.1.39" && result.text.find("Iba pues Jesus") == 0)
			found = true;
	check(found, "FTS search did not find corrected Mark 1:39");
	query.text = "\"ya no podia Jesus\"";
	found = false;
	for (const auto &result : backend.search(module, query))
		if (result.osisRef == "Mark.1.45" && result.text.find("ya no podia Jesus") != std::string::npos)
			found = true;
	check(found, "FTS search did not find corrected Mark 1:45");
	query.text = "\"llamar á convertir\"";
	found = false;
	for (const auto &result : backend.search(module, query))
		if (result.osisRef == "Mark.2.17" && result.text.find("llamar á convertir") != std::string::npos)
			found = true;
	check(found, "FTS search did not find corrected Mark 2:17");
	query.text = "\"arrojando al mar el trigo\"";
	found = false;
	for (const auto &result : backend.search(module, query)) {
		if (result.osisRef == "Acts.27.38") found = true;
		check(result.osisRef != "Acts.27.30", "grain remains indexed under Acts 27:30");
	}
	check(found, "FTS search did not find recovered Acts 27:38");
	query.mode = BibleSearchMode::Regex;
	query.text = "\\$";
	check(backend.search(module, query).empty(), "a dollar OCR error remains in SQLite");
	std::printf("torresamat_contexto_sqlite_failures=%d\n", failures);
	return failures ? 1 : 0;
}
