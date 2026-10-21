#include "notebookpackage.h"

#include "zipstore.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

QString IntegrityReport::summary() const {
    QString text = QStringLiteral("%1 pages, %2 media files").arg(pagesFound).arg(mediaFound);
    if (rebuiltIndex)
        text += QStringLiteral(", index rebuilt from folders");
    if (!missingEntries.isEmpty())
        text += QStringLiteral(", MISSING: %1").arg(missingEntries.join(QStringLiteral(", ")));
    if (!brokenLinks.isEmpty())
        text += QStringLiteral(", broken links: %1").arg(brokenLinks.join(QStringLiteral(", ")));
    if (missingEntries.isEmpty() && brokenLinks.isEmpty())
        text += QStringLiteral(", all present");
    return text;
}

QString NotebookPackage::sanitizeTitle(const QString& title) {
    QString safe;
    safe.reserve(title.size());
    for (QChar ch : title) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\') || ch == QLatin1Char(':')
            || ch == QLatin1Char('*') || ch == QLatin1Char('?') || ch == QLatin1Char('"')
            || ch == QLatin1Char('<') || ch == QLatin1Char('>') || ch == QLatin1Char('|')
            || ch.category() == QChar::Other_Control) {
            safe.append(QLatin1Char('-'));
        } else {
            safe.append(ch);
        }
    }
    safe = safe.trimmed();
    while (safe.startsWith(QLatin1Char('.')))
        safe.remove(0, 1);
    if (safe.size() > 80)
        safe = safe.left(80).trimmed();
    if (safe.isEmpty())
        safe = QStringLiteral("Untitled");
    return safe;
}

// Pick a name not already in `used`, suffixing " (2)", " (3)", ... on collision.
static QString uniqueName(const QString& base, QSet<QString>& used) {
    QString candidate = base;
    int n = 2;
    while (used.contains(candidate)) {
        candidate = QStringLiteral("%1 (%2)").arg(base).arg(n);
        ++n;
    }
    used.insert(candidate);
    return candidate;
}

struct PageLocation {
    QString file;       // zip path of the page JSON
    QString mediaDir;   // zip path prefix of the page's media folder (no trailing '/')
};

static QJsonObject buildIndex(const Notebook& notebook, QMap<QString, PageLocation>* locations) {
    QJsonArray sectionsJson;
    QSet<QString> usedSectionDirs;
    for (const Section& section : notebook.sections) {
        const QString sectionDir = uniqueName(NotebookPackage::sanitizeTitle(section.title), usedSectionDirs);
        QJsonArray pagesJson;
        QSet<QString> usedPageNames;
        for (const Page& page : section.pages) {
            const QString pageBase = uniqueName(NotebookPackage::sanitizeTitle(page.title), usedPageNames);
            PageLocation location;
            location.file = QStringLiteral("%1/%2.json").arg(sectionDir, pageBase);
            location.mediaDir = QStringLiteral("%1/%2.media").arg(sectionDir, pageBase);
            if (locations != nullptr)
                locations->insert(page.id, location);
            QJsonObject pageJson;
            pageJson[QStringLiteral("id")] = page.id;
            pageJson[QStringLiteral("title")] = page.title;
            pageJson[QStringLiteral("file")] = location.file;
            pageJson[QStringLiteral("mediaDir")] = location.mediaDir;
            pagesJson.append(pageJson);
        }
        QJsonObject sectionJson;
        sectionJson[QStringLiteral("id")] = section.id;
        sectionJson[QStringLiteral("title")] = section.title;
        sectionJson[QStringLiteral("dir")] = sectionDir;
        sectionJson[QStringLiteral("pages")] = pagesJson;
        sectionsJson.append(sectionJson);
    }
    QJsonObject index;
    index[QStringLiteral("format")] = QStringLiteral("qtnotebook");
    index[QStringLiteral("version")] = NotebookPackage::kFormatVersion;
    index[QStringLiteral("title")] = notebook.title;
    index[QStringLiteral("sections")] = sectionsJson;
    return index;
}

static QJsonObject pageToJson(const Page& page) {
    QJsonArray elementsJson;
    for (const QJsonObject& element : page.elements)
        elementsJson.append(element);
    QJsonObject json;
    json[QStringLiteral("format")] = QStringLiteral("qtnote-page");
    json[QStringLiteral("version")] = NotebookPackage::kFormatVersion;
    json[QStringLiteral("pageId")] = page.id;
    json[QStringLiteral("title")] = page.title;
    json[QStringLiteral("elements")] = elementsJson;
    return json;
}

// Scan zip entries and derive a Notebook structure when notebook.json is
// unusable. Page GUIDs are freshly generated — old internal links to a
// rebuilt notebook will report as broken, which is honest: the index that
// carried the GUIDs is gone.
static bool rebuildFromEntries(const QMap<QString, QByteArray>& entries, Notebook* notebook, QString* error) {
    QMap<QString, Section> sectionsByDir;   // dir name -> section, ordered by name via QMap
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const QString name = it.key();
        if (name == QStringLiteral("notebook.json") || !name.endsWith(QStringLiteral(".json")))
            continue;
        const QStringList parts = name.split(QLatin1Char('/'));
        QString sectionTitle;
        QString pageTitle;
        if (parts.size() == 2) {
            sectionTitle = parts.at(0);
            pageTitle = parts.at(1).chopped(5);   // strip ".json"
        } else if (parts.size() == 1) {
            sectionTitle = QStringLiteral("Unsorted");
            pageTitle = parts.at(0).chopped(5);
        } else {
            continue;   // media JSON or something else — not a page
        }
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(it.value(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            continue;
        Section& section = sectionsByDir[sectionTitle];
        if (section.id.isEmpty()) {
            section.id = Notebook::newId();
            section.title = sectionTitle;
        }
        Page page;
        page.id = doc.object().value(QStringLiteral("pageId")).toString();
        if (page.id.isEmpty())
            page.id = Notebook::newId();
        page.title = doc.object().value(QStringLiteral("title")).toString(pageTitle);
        const QJsonArray elementsJson = doc.object().value(QStringLiteral("elements")).toArray();
        for (const QJsonValue& value : elementsJson)
            page.elements.append(value.toObject());
        const QString mediaPrefix = (parts.size() == 2 ? parts.at(0) + QLatin1Char('/') : QString())
            + pageTitle + QStringLiteral(".media/");
        for (auto mediaIt = entries.constBegin(); mediaIt != entries.constEnd(); ++mediaIt) {
            if (mediaIt.key().startsWith(mediaPrefix) && mediaIt.key().size() > mediaPrefix.size())
                page.media.insert(mediaIt.key().mid(mediaPrefix.size()), mediaIt.value());
        }
        section.pages.append(page);
    }
    if (sectionsByDir.isEmpty()) {
        if (error != nullptr)
            *error = QStringLiteral("No notebook index and no pages found in the file");
        return false;
    }
    notebook->sections = sectionsByDir.values();
    notebook->title = QStringLiteral("Recovered Notebook");
    return true;
}

bool NotebookPackage::saveAtomic(const Notebook& notebook, const QString& filePath, QString* error) {
    QMap<QString, PageLocation> locations;
    const QJsonObject index = buildIndex(notebook, &locations);

    QMap<QString, QByteArray> entries;
    entries.insert(QStringLiteral("notebook.json"),
                   QJsonDocument(index).toJson(QJsonDocument::Indented));
    for (const Section& section : notebook.sections) {
        for (const Page& page : section.pages) {
            const PageLocation location = locations.value(page.id);
            entries.insert(location.file,
                           QJsonDocument(pageToJson(page)).toJson(QJsonDocument::Indented));
            for (auto it = page.media.constBegin(); it != page.media.constEnd(); ++it)
                entries.insert(location.mediaDir + QLatin1Char('/') + it.key(), it.value());
        }
    }

    const QString tempPath = filePath + QStringLiteral(".tmp");
    if (!ZipStore::writeFile(tempPath, entries, error))
        return false;

    // Verify: reopen the temp file and confirm the entry list matches.
    QStringList writtenNames;
    if (!ZipStore::listEntries(tempPath, &writtenNames, error)
        || writtenNames.size() != entries.size()) {
        QFile::remove(tempPath);
        if (error != nullptr && error->isEmpty())
            *error = QStringLiteral("Verification failed for %1").arg(tempPath);
        return false;
    }

    // Keep the previous good file as .bak, then move the verified temp in.
    const QString backupPath = filePath + QStringLiteral(".bak");
    if (QFile::exists(filePath)) {
        QFile::remove(backupPath);
        if (!QFile::copy(filePath, backupPath)) {
            QFile::remove(tempPath);
            if (error != nullptr)
                *error = QStringLiteral("Could not back up %1 to %2").arg(filePath, backupPath);
            return false;
        }
        QFile::remove(filePath);
    }
    if (!QFile::rename(tempPath, filePath)) {
        // Try to put the original back — never leave the user with nothing.
        if (QFile::exists(backupPath))
            QFile::rename(backupPath, filePath);
        if (error != nullptr)
            *error = QStringLiteral("Could not move %1 into place as %2").arg(tempPath, filePath);
        return false;
    }
    return true;
}

bool NotebookPackage::load(const QString& filePath, Notebook* notebook, QString* error,
                           IntegrityReport* report) {
    QMap<QString, QByteArray> entries;
    if (!ZipStore::readFile(filePath, &entries, error))
        return false;

    Notebook loaded;
    bool rebuilt = false;
    bool indexOk = false;
    const QByteArray indexBytes = entries.value(QStringLiteral("notebook.json"));
    if (!indexBytes.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument indexDoc = QJsonDocument::fromJson(indexBytes, &parseError);
        if (parseError.error == QJsonParseError::NoError && indexDoc.isObject()
            && indexDoc.object().value(QStringLiteral("format")).toString() == QStringLiteral("qtnotebook")) {
            indexOk = true;
            const QJsonObject index = indexDoc.object();
            loaded.title = index.value(QStringLiteral("title")).toString(loaded.title);
            const QJsonArray sectionsJson = index.value(QStringLiteral("sections")).toArray();
            for (const QJsonValue& sectionValue : sectionsJson) {
                const QJsonObject sectionJson = sectionValue.toObject();
                Section section;
                section.id = sectionJson.value(QStringLiteral("id")).toString(Notebook::newId());
                section.title = sectionJson.value(QStringLiteral("title")).toString();
                const QJsonArray pagesJson = sectionJson.value(QStringLiteral("pages")).toArray();
                for (const QJsonValue& pageValue : pagesJson) {
                    const QJsonObject pageJson = pageValue.toObject();
                    Page page;
                    page.id = pageJson.value(QStringLiteral("id")).toString(Notebook::newId());
                    page.title = pageJson.value(QStringLiteral("title")).toString();
                    const QString file = pageJson.value(QStringLiteral("file")).toString();
                    const QString mediaDir = pageJson.value(QStringLiteral("mediaDir")).toString();
                    const QByteArray pageBytes = entries.value(file);
                    if (!pageBytes.isEmpty()) {
                        const QJsonDocument pageDoc = QJsonDocument::fromJson(pageBytes);
                        const QJsonArray elementsJson = pageDoc.object().value(QStringLiteral("elements")).toArray();
                        for (const QJsonValue& elementValue : elementsJson)
                            page.elements.append(elementValue.toObject());
                    }
                    const QString mediaPrefix = mediaDir + QLatin1Char('/');
                    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
                        if (it.key().startsWith(mediaPrefix) && it.key().size() > mediaPrefix.size())
                            page.media.insert(it.key().mid(mediaPrefix.size()), it.value());
                    }
                    section.pages.append(page);
                }
                loaded.sections.append(section);
            }
        }
    }
    if (!indexOk) {
        if (!rebuildFromEntries(entries, &loaded, error))
            return false;
        rebuilt = true;
    }

    *notebook = loaded;
    if (report != nullptr) {
        *report = integrityCheck(filePath);
        report->rebuiltIndex = report->rebuiltIndex || rebuilt;
    }
    return true;
}

IntegrityReport NotebookPackage::integrityCheck(const QString& filePath) {
    IntegrityReport report;
    QMap<QString, QByteArray> entries;
    QString error;
    if (!ZipStore::readFile(filePath, &entries, &error)) {
        report.missingEntries.append(QStringLiteral("unreadable file: %1").arg(error));
        return report;
    }

    QSet<QString> pageIds;
    QList<QPair<QString, QString>> expectedPages;   // page file, media dir
    bool indexOk = false;
    const QByteArray indexBytes = entries.value(QStringLiteral("notebook.json"));
    if (!indexBytes.isEmpty()) {
        const QJsonDocument indexDoc = QJsonDocument::fromJson(indexBytes);
        if (indexDoc.isObject()) {
            indexOk = true;
            const QJsonArray sectionsJson = indexDoc.object().value(QStringLiteral("sections")).toArray();
            for (const QJsonValue& sectionValue : sectionsJson) {
                const QJsonArray pagesJson = sectionValue.toObject().value(QStringLiteral("pages")).toArray();
                for (const QJsonValue& pageValue : pagesJson) {
                    const QJsonObject pageJson = pageValue.toObject();
                    pageIds.insert(pageJson.value(QStringLiteral("id")).toString());
                    expectedPages.append(qMakePair(pageJson.value(QStringLiteral("file")).toString(),
                                                   pageJson.value(QStringLiteral("mediaDir")).toString()));
                }
            }
        }
    }
    if (!indexOk)
        report.rebuiltIndex = true;   // load() would rebuild from the folder scan

    // Pages present on disk (by file), regardless of index state.
    QSet<QString> pageFiles;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const QString name = it.key();
        if (name.endsWith(QStringLiteral(".json")) && name != QStringLiteral("notebook.json")
            && !name.contains(QStringLiteral(".media/")))
            pageFiles.insert(name);
    }
    report.pagesFound = pageFiles.size();
    for (const QPair<QString, QString>& expected : expectedPages) {
        if (!entries.contains(expected.first))
            report.missingEntries.append(expected.first);
    }

    // Media files: any entry under a "...media/" folder.
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        if (it.key().contains(QStringLiteral(".media/")))
            ++report.mediaFound;
    }

    // Referenced media that is missing, and broken internal links, are found
    // by scanning each page JSON.
    const QRegularExpression linkPattern(QStringLiteral("note://page/([0-9a-fA-F-]{36})"));
    QSet<QString> broken;
    for (const QString& pageFile : pageFiles) {
        const QByteArray pageBytes = entries.value(pageFile);
        const QJsonDocument pageDoc = QJsonDocument::fromJson(pageBytes);
        if (!pageDoc.isObject())
            continue;
        // Determine this page's media dir from its file path.
        const QString mediaDir = pageFile.chopped(5) + QStringLiteral(".media");
        const QJsonArray elementsJson = pageDoc.object().value(QStringLiteral("elements")).toArray();
        for (const QJsonValue& elementValue : elementsJson) {
            const QJsonObject element = elementValue.toObject();
            if (element.value(QStringLiteral("kind")).toString() == QStringLiteral("image")) {
                const QString media = element.value(QStringLiteral("media")).toString();
                if (!media.isEmpty() && !entries.contains(mediaDir + QLatin1Char('/') + media)
                    && !report.missingEntries.contains(mediaDir + QLatin1Char('/') + media))
                    report.missingEntries.append(mediaDir + QLatin1Char('/') + media);
            }
        }
        const QString pageText = QString::fromUtf8(pageBytes);
        QRegularExpressionMatchIterator matches = linkPattern.globalMatch(pageText);
        while (matches.hasNext()) {
            const QString target = matches.next().captured(1).toLower();
            bool found = false;
            for (const QString& id : pageIds) {
                if (id.toLower() == target) {
                    found = true;
                    break;
                }
            }
            if (!found)
                broken.insert(target);
        }
    }
    report.brokenLinks = broken.values();
    report.brokenLinks.sort();
    report.missingEntries.sort();
    report.ok = report.missingEntries.isEmpty();
    return report;
}
