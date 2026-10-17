#ifndef BIBLIA_ELIM_STUDY_BIBLE_PDF_H
#define BIBLIA_ELIM_STUDY_BIBLE_PDF_H

#include <functional>
#include <string>

struct StudyBiblePdfOptions {
    bool contributionsOnly = false;
    // Called on the exporting thread; no GTK access is permitted here.
    std::function<bool()> cancelled;
};

struct StudyBiblePdfStats {
    int books = 0, verses = 0, contributions = 0, pages = 0;
};

// Reads a saved edition without migrating it. Replaces output atomically only
// after Cairo finishes successfully. Does not require a display or PDF viewer.
bool exportStudyBiblePdf(const std::string &modulePath, const std::string &output,
                        const StudyBiblePdfOptions &options,
                        StudyBiblePdfStats &stats, std::string &error);

#endif
