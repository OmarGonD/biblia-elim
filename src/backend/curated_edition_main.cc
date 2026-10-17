#include "backend/editable_edition.h"
#include "backend/bible_book_map.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {
std::string stringValue(JsonObject *object, const char *key)
{
	JsonNode *node = json_object_get_member(object, key);
	if (!node || !JSON_NODE_HOLDS_VALUE(node) ||
	    json_node_get_value_type(node) != G_TYPE_STRING)
		throw std::runtime_error(std::string("expected string: ") + key);
	return json_node_get_string(node);
}

int positiveInt(JsonObject *object, const char *key)
{
	JsonNode *node = json_object_get_member(object, key);
	if (!node || !JSON_NODE_HOLDS_VALUE(node) ||
	    json_node_get_value_type(node) != G_TYPE_INT64)
		throw std::runtime_error(std::string("expected integer: ") + key);
	gint64 value = json_node_get_int(node);
	if (value < 1 || value > G_MAXINT)
		throw std::runtime_error(std::string("invalid reference: ") + key);
	return static_cast<int>(value);
}

JsonArray *arrayValue(JsonObject *object, const char *key)
{
	JsonNode *node = json_object_get_member(object, key);
	if (!node || !JSON_NODE_HOLDS_ARRAY(node))
		throw std::runtime_error(std::string("expected array: ") + key);
	return json_node_get_array(node);
}

JsonObject *element(JsonArray *array, guint i)
{
	JsonNode *node = json_array_get_element(array, i);
	if (!node || !JSON_NODE_HOLDS_OBJECT(node))
		throw std::runtime_error("expected object in array");
	return json_node_get_object(node);
}

int bookId(JsonObject *object)
{
	const auto book = findBibleBookByAnyName(stringValue(object, "book"));
	if (!book) throw std::runtime_error("unknown book");
	return book->bookId;
}

void require(bool ok, const std::string &error)
{
	if (!ok) throw std::runtime_error(error);
}

/* Private sibling directory keeps unfinished projects outside the module
 * catalog, and guarantees the final move stays on the same filesystem. */
struct Staging {
	std::string directory, file;
	~Staging() {
		if (!file.empty()) { g_unlink(file.c_str()); g_unlink((file + ".tmp").c_str()); }
		if (!directory.empty()) g_rmdir(directory.c_str());
	}
};
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		g_printerr("Usage: biblia-curated-edition SOURCE.sqlite OUTPUT.sqlite PLAN.json\n");
		return 2;
	}
	auto parser = std::unique_ptr<JsonParser, decltype(&g_object_unref)>(json_parser_new(), g_object_unref);
	Staging staging;
	try {
		GError *message = nullptr;
		if (!json_parser_load_from_file(parser.get(), argv[3], &message)) {
			std::string error = message ? message->message : "cannot read plan";
			g_clear_error(&message); throw std::runtime_error(error);
		}
		JsonNode *root = json_parser_get_root(parser.get());
		if (!root || !JSON_NODE_HOLDS_OBJECT(root)) throw std::runtime_error("invalid plan root");
		JsonObject *plan = json_node_get_object(root);
		require(stringValue(plan, "format") == "biblia-elim-curated-edition-1", "unsupported plan format");
		const auto id = stringValue(plan, "project_id"), name = stringValue(plan, "name");
		JsonArray *passages = arrayValue(plan, "passages");
		JsonArray *books = arrayValue(plan, "book_notes");
		JsonArray *glossary = arrayValue(plan, "glossary");
		require(!g_file_test(argv[2], G_FILE_TEST_EXISTS), "destination already exists");
		gchar *directory = g_strdup_printf("%s.build-XXXXXX", argv[2]);
		if (!g_mkdtemp(directory)) { g_free(directory); throw std::runtime_error("cannot create staging directory"); }
		staging.directory = directory; g_free(directory);
		staging.file = staging.directory + "/project.sqlite";
		std::string error;
		require(EditableEdition::createFromModule(argv[1], staging.file, id, name, error), error);
		auto edition = EditableEdition::open(staging.file, error);
		require(bool(edition), error);
		require(edition->metadata().sourceModuleId == stringValue(plan, "source_id"), "source ID mismatch");
		require(edition->metadata().sourceFingerprint == stringValue(plan, "source_sha256"), "source fingerprint mismatch");
		if (json_object_has_member(plan, "language"))
			require(edition->metadata().language == stringValue(plan, "language"), "source language mismatch");
		const bool english = edition->metadata().language == "en";
		const std::string noteHeader = json_object_has_member(plan, "comment_notice")
			? stringValue(plan, "comment_notice") : name + (english ? " · editorial draft" : " · borrador editorial");
		std::set<std::tuple<int,int,int>> seen;
		guint changed = 0;
		for (guint i = 0; i < json_array_get_length(passages); ++i) {
			JsonObject *p = element(passages, i);
			BibleReference ref{0, bookId(p), positiveInt(p, "chapter"), positiveInt(p, "verse")};
			require(seen.emplace(ref.book, ref.chapter, ref.verse).second, "duplicate passage");
			std::string base;
			require(edition->verseText(ref, base, error), error);
			const auto expected = stringValue(p, json_object_has_member(p, "expected_current") ? "expected_current" : "expected_base");
			require(base == expected, "source text mismatch at passage " + std::to_string(i));
			const auto action = stringValue(p, "action");
			require(action == "revise" || action == "annotate" || action == "propose", "invalid editorial action");
			EditionDecision decision;
			decision.reference = ref;
			decision.proposal = stringValue(p, "text");
			decision.rationale = stringValue(p, "rationale");
			decision.evidence = stringValue(p, "evidence");
			decision.confidence = stringValue(p, "confidence");
			decision.status = "Pendiente de revisión";
			require(!decision.rationale.empty() && !decision.evidence.empty(), "every passage requires rationale and evidence");
			if (action == "revise") {
				require(!decision.proposal.empty(), "revised verse cannot be empty");
				require(edition->setVerseText(ref, decision.proposal, error), error);
				if (decision.proposal != base) ++changed;
			}
			require(edition->setDecision(decision, error), error);
			std::string note = noteHeader + "\n";
			const std::string actionLabel = action == "revise" ? (english ? "Revised draft" : "Borrador revisado")
				: action == "propose" ? (english ? "Unapplied proposal" : "Propuesta sin aplicar")
				: (english ? "Study note" : "Nota de estudio");
			note += (english ? "Action: " : "Acción: ") + actionLabel;
			note += "\n" + edition->metadata().sourceName + ": " + stringValue(p, "expected_base") + "\n";
			if (!decision.proposal.empty()) note += std::string(english ? "Proposal: " : "Propuesta: ") + decision.proposal + "\n";
			std::string confidence = decision.confidence;
			if (english) {
				if (confidence == "Alta") confidence = "High";
				else if (confidence == "Media") confidence = "Medium";
				else if (confidence == "Baja") confidence = "Low";
			}
			note += decision.rationale + (english ? "\nSources:\n" : "\nFuentes:\n") + decision.evidence;
			note += std::string(english ? "\nConfidence: " : "\nConfianza: ") + confidence;
			note += english ? "; pending human review." : "; pendiente de revisión humana.";
			if (json_object_has_member(p, "comment")) note = stringValue(p, "comment");
			require(edition->setVerseComment(ref, note, error), error);
		}
		std::set<int> notedBooks;
		for (guint i = 0; i < json_array_get_length(books); ++i) {
			JsonObject *b = element(books, i); int book = bookId(b);
			require(notedBooks.insert(book).second, "duplicate book note");
			require(edition->setBookComment(book, stringValue(b, "comment"), error), error);
		}
		for (guint i = 0; i < json_array_get_length(glossary); ++i) {
			JsonObject *g = element(glossary, i);
			require(edition->setGlossaryEntry({stringValue(g,"source_term"), stringValue(g,"target_term"), stringValue(g,"note")},error),error);
		}
		edition.reset();
		GFile *from = g_file_new_for_path(staging.file.c_str()), *to = g_file_new_for_path(argv[2]);
		bool moved = g_file_move(from, to, G_FILE_COPY_NONE, nullptr, nullptr, nullptr, &message);
		g_object_unref(from); g_object_unref(to);
		if (!moved) { std::string e = message ? message->message : "cannot finalize project"; g_clear_error(&message); throw std::runtime_error(e); }
		g_print("Created %s: %u editorial passages, %u revised verses; all pending human review.\n", name.c_str(), json_array_get_length(passages), changed);
		return 0;
	} catch (const std::exception &e) {
		g_printerr("Creation failed: %s\n", e.what());
		return 1;
	}
}
