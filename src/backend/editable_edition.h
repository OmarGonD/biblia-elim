#ifndef BIBLIA_ELIM_EDITABLE_EDITION_H
#define BIBLIA_ELIM_EDITABLE_EDITION_H

#include <memory>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "backend/sqlite/sqlite_module_writer.h"

struct EditableEditionMetadata {
	std::string projectId;
	std::string name;
	std::string language;
	std::string versification;
	std::string sourceModuleId;
	std::string sourceName;
	std::string sourceLicense;
	std::string sourceAttribution;
	std::string sourceFingerprint;
};

struct EditionRevision {
	int book = 0, chapter = 0, verse = 0;
	std::string oldText, newText;
	std::int64_t modified = 0;
};

struct EditionGlossaryEntry {
	std::string sourceTerm, targetTerm, note;
};

struct EditionDecision {
	BibleReference reference;
	std::string proposal, rationale, evidence, confidence, status;
};

struct EditionComparison {
	BibleReference reference;
	std::string bookName, baseText, currentText, comment;
	std::string proposal, rationale, evidence, status;
	bool edited() const { return baseText != currentText; }
	bool commented() const { return comment.find_first_not_of(" \t\r\n") != std::string::npos; }
	bool pending() const { return !proposal.empty() && (status == "Pendiente de revisión" || status == "pendiente de revisión"); }
};

/* Persistent, incrementally editable complete Bible project. This is a
 * working format; installation/export as a read-only module is a later step. */
class EditableEdition {
public:
	static bool sourceMayBeDerived(const std::string &moduleId,
				       const std::string &license);
	static bool create(const std::string &path,
			   const EditableEditionMetadata &metadata,
			   const std::vector<SqliteImportBook> &books,
			   const std::vector<SqliteImportVerse> &verses,
			   std::string &error);
	/* Copy an approved source or an existing project. Project copies retain
	 * original source provenance, annotations, decisions and revision history. */
	static bool createFromModule(const std::string &sourcePath,
				     const std::string &path,
				     const std::string &projectId,
				     const std::string &name,
				     std::string &error);
	static std::unique_ptr<EditableEdition> open(const std::string &path,
						     std::string &error);
	/* Read-only: supports legacy projects without migrating or locking them.
	 * Zero book/chapter selects all references. Original text is immutable. */
	static bool comparisons(const std::string &path, EditableEditionMetadata &metadata,
		std::vector<EditionComparison> &verses, std::string &error,
		int book = 0, int chapter = 0);
	~EditableEdition();
	EditableEdition(const EditableEdition &) = delete;
	EditableEdition &operator=(const EditableEdition &) = delete;

	const EditableEditionMetadata &metadata() const { return metadata_; }
	bool verseText(const BibleReference &reference, std::string &text,
		       std::string &error) const;
	bool setName(const std::string &name, std::string &error);
	bool setVerseText(const BibleReference &reference, const std::string &text,
			  std::string &error);
	bool setBookComment(int bookId, const std::string &comment,
			    std::string &error);
	bool bookComment(int bookId, std::string &comment, std::string &error) const;
	bool setVerseComment(const BibleReference &reference,
			     const std::string &comment, std::string &error);
	bool verseComment(const BibleReference &reference, std::string &comment,
			   std::string &error) const;
	bool glossaryEntries(std::vector<EditionGlossaryEntry> &entries,
			     std::string &error) const;
	bool setGlossaryEntry(const EditionGlossaryEntry &entry, std::string &error);
	bool setDecision(const EditionDecision &decision, std::string &error);
	bool decision(const BibleReference &reference, EditionDecision &decision,
		      std::string &error) const;
	bool revisions(const BibleReference &reference,
		       std::vector<EditionRevision> &revisions,
		       std::string &error) const;

private:
	EditableEdition(sqlite3 *db, EditableEditionMetadata metadata)
		: db_(db), metadata_(std::move(metadata)) {}
	bool setComment(int bookId, int chapter, int verse,
			const std::string &comment, std::string &error);
	bool getComment(const char *sql, int bookId, int chapter, int verse,
			std::string &comment, std::string &error) const;
	sqlite3 *db_ = nullptr;
	EditableEditionMetadata metadata_;
};

#endif
