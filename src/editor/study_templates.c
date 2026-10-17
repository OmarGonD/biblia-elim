#include "study_templates.h"

/* Use semantic blocks so prompts remain ordinary, editable paragraphs. */
static const StudyTemplate templates[] = {
	{
		"Estudio inductivo",
		"Observa el pasaje, interpreta su sentido y prepara una aplicación concreta.",
		"<h1>Estudio inductivo</h1>"
		"<p><b>Pasaje:</b> [Referencia y versión bíblica]</p>"
		"<p><b>Fecha y tema:</b> [Completar]</p>"
		"<h2>1. Contexto</h2>"
		"<p>[¿Quién escribe, a quién y en qué situación? ¿Qué ocurre antes y después?]</p>"
		"<h2>2. Observación · ¿Qué dice?</h2>"
		"<p>[Anota personajes, acciones, palabras repetidas, contrastes y conexiones. Cita los versículos.]</p>"
		"<h2>3. Interpretación · ¿Qué significa?</h2>"
		"<p>[Explica el mensaje para sus primeros lectores. Registra referencias paralelas y preguntas pendientes.]</p>"
		"<h2>4. Idea principal</h2>"
		"<p>[Resume el mensaje del pasaje en una frase.]</p>"
		"<h2>5. Aplicación · ¿Cómo responderé?</h2>"
		"<p>[Define una acción concreta, cuándo la realizarás y cómo evaluarás tu respuesta.]</p>"
		"<h2>6. Oración</h2>"
		"<p>[Responde a Dios a partir de lo aprendido.]</p>"
		"<h2>Fuentes y notas</h2><p>[Anota las obras consultadas y sus referencias.]</p>"
	},
	{
		"Sermón",
		"Desarrolla una idea central con exposición bíblica, aplicaciones y conclusión.",
		"<h1>[Título del sermón]</h1>"
		"<p><b>Texto base:</b> [Pasaje y versión bíblica]</p>"
		"<p><b>Audiencia y fecha:</b> [Completar]</p>"
		"<p><b>Idea central:</b> [El mensaje del pasaje en una frase]</p>"
		"<p><b>Propósito:</b> [¿Qué esperas que los oyentes comprendan y pongan en práctica?]</p>"
		"<h2>Introducción</h2>"
		"<p>[Presenta la necesidad, sitúa el pasaje en su contexto y conecta con la idea central.]</p>"
		"<h2>I. [Primer punto]</h2>"
		"<p><b>Exposición:</b> [Explica el texto y sus versículos de apoyo.]</p>"
		"<p><b>Ilustración y aplicación:</b> [Incluye un ejemplo y una respuesta concreta.]</p>"
		"<h2>II. [Segundo punto]</h2>"
		"<p><b>Exposición:</b> [Desarrolla el argumento desde el pasaje.]</p>"
		"<p><b>Ilustración y aplicación:</b> [Conecta con la vida de los oyentes.]</p>"
		"<h2>III. [Tercer punto]</h2>"
		"<p><b>Exposición:</b> [Completa el desarrollo de la idea central.]</p>"
		"<p><b>Ilustración y aplicación:</b> [Propón una respuesta al mensaje.]</p>"
		"<h2>Conclusión y oración</h2>"
		"<p>[Retoma la idea central, resume la respuesta esperada y prepara la oración final.]</p>"
		"<h2>Fuentes y notas</h2><p>[Registra citas, autores y referencias utilizadas.]</p>"
	},
	{
		"Estudio devocional",
		"Relaciona la lectura con tu vida, una respuesta práctica y la oración.",
		"<h1>Mi devocional</h1>"
		"<p><b>Fecha:</b> [Completar]</p><p><b>Lectura:</b> [Pasaje y versión bíblica]</p>"
		"<h2>Un versículo para recordar</h2>"
		"<p>[Escribe el versículo y su referencia.]</p>"
		"<h2>¿Qué revela de Dios?</h2>"
		"<p>[Observa su carácter, sus acciones y sus promesas en el contexto del pasaje.]</p>"
		"<h2>¿Qué ilumina en mi vida?</h2>"
		"<p>[Identifica una actitud, una necesidad o un motivo de gratitud.]</p>"
		"<h2>Mi respuesta de hoy</h2>"
		"<p>[Escribe una decisión concreta y cuándo la pondrás en práctica.]</p>"
		"<h2>Oración</h2><p>[Expresa tu gratitud y tus peticiones.]</p>"
		"<h2>Para volver a revisar</h2><p>[Anota una pregunta o un recordatorio para los próximos días.]</p>"
	},
	{
		"Lectura y reflexión",
		"Organiza tus notas, preguntas y conclusiones durante la lectura bíblica.",
		"<h1>Lectura y reflexión</h1>"
		"<p><b>Pasaje:</b> [Referencia y versión bíblica]</p><p><b>Fecha:</b> [Completar]</p>"
		"<h2>Resumen de la lectura</h2><p>[Describe lo que sucede y el mensaje principal.]</p>"
		"<h2>Palabras y versículos destacados</h2><p>[Registra las frases que deseas estudiar con sus referencias.]</p>"
		"<h2>Preguntas para investigar</h2><p>[Anota tus preguntas y qué fuentes necesitas consultar.]</p>"
		"<h2>Conexiones con otros pasajes</h2><p>[Relaciona las referencias y explica su conexión.]</p>"
		"<h2>Conclusión y respuesta personal</h2><p>[Resume lo aprendido y el siguiente paso que darás.]</p>"
		"<h2>Fuentes y notas</h2><p>[Registra las fuentes consultadas.]</p>"
	}
};

guint study_template_count(void) { return G_N_ELEMENTS(templates); }

const StudyTemplate *study_template_get(guint index)
{
	return index < G_N_ELEMENTS(templates) ? &templates[index] : NULL;
}
