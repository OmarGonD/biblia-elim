// Texto plano de un módulo SWORD para versículos dados en numeración estándar (KJV).
// Usa la biblioteca de SWORD: filtros de texto plano (sin notas ni marcado) y el mapeo
// entre versificaciones (p. ej. TorresAmat usa Vulg; mod2imp/diatheke NO mapean).
// Entrada (stdin): una referencia OSIS por línea (John.1.18). Salida: ref \t ref_en_el_módulo \t texto
// Compilar: g++ -O2 -o swordtext swordtext.cc -lsword
#include <swmgr.h>
#include <swmodule.h>
#include <versekey.h>
#include <markupfiltmgr.h>
#include <iostream>
#include <string>

using namespace sword;

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cerr << "uso: swordtext MODULO < refs\n";
		return 2;
	}
	SWMgr mgr(new MarkupFilterMgr(FMT_PLAIN));
	SWModule *m = mgr.getModule(argv[1]);
	if (!m) {
		std::cerr << "módulo no encontrado: " << argv[1] << "\n";
		return 1;
	}
	std::string linea;
	while (std::getline(std::cin, linea)) {
		if (linea.empty()) continue;
		VerseKey k;
		k.setVersificationSystem("KJV");
		k.setText(linea.c_str());
		if (k.popError()) {
			std::cout << linea << "\t\t\n";
			continue;
		}
		m->setKey(k);
		VerseKey *mk = (VerseKey *)m->getKey();
		std::string texto = m->stripText();
		for (char &c : texto)
			if (c == '\n' || c == '\t' || c == '\r') c = ' ';
		std::cout << linea << "\t" << mk->getOSISRef() << "\t" << texto << "\n";
	}
	return 0;
}
