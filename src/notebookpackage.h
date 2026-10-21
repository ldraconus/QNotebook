#pragma once

// notebookpackage.h — the .qnb file format: one file, ZIP inside.
//
// Directory structure inside the zip IS the notebook structure:
//   notebook.json                              (index, see NotebookPackage)
//   <SectionTitle>/<PageTitle>.json            (page canvas JSON)
//   <SectionTitle>/<PageTitle>.media/<files>   (that page's images / PDF pages)
//
// The index stores page GUIDs and tab/page order. If the index is missing or
// corrupt, load() rebuilds it by scanning the zip folders (and says so in the
// IntegrityReport) — the folders are the fallback source of truth.

#include "notebook.h"

#include <QString>
#include <QStringList>

struct IntegrityReport
{
    bool ok = false;
    bool rebuiltIndex = false;
    int pagesFound = 0;
    int mediaFound = 0;
    QStringList missingEntries;    // expected zip entries not present / unreadable
    QStringList brokenLinks;       // note://page/<guid> targets not in the index
    QString summary() const;
};

class NotebookPackage
{
public:
    static const int kFormatVersion = 1;

    // Write notebook to filePath atomically: write <file>.tmp, verify the
    // temp file reopens with the expected entry count, keep the previous
    // file as <file>.bak, then replace. On any failure the original file
    // (if any) is left untouched and *error says what happened — no silent loss.
    static bool saveAtomic(const Notebook& notebook, const QString& filePath, QString* error);

    // Load. On index problems, falls back to scanning the zip structure.
    static bool load(const QString& filePath, Notebook* notebook, QString* error,
                     IntegrityReport* report = nullptr);

    // Standalone verification of a .qnb file (also used by load()).
    static IntegrityReport integrityCheck(const QString& filePath);

    // Make a section/page title safe as a single zip path component.
    static QString sanitizeTitle(const QString& title);
};
