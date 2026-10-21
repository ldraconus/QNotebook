// smoke.cpp — headless verification for the QtNotebook scaffold.
//
// Builds a notebook programmatically (2 sections, formatted text + a real
// PNG image element), saves it as a .qnb via NotebookPackage::saveAtomic,
// runs integrityCheck, loads it back, and asserts the round trip. Also
// verifies broken-link detection on a second, deliberately damaged copy.
// Run with: QT_QPA_PLATFORM=offscreen ./qtnotebook_smoke
// Exit code 0 and "SMOKE PASS" on success; failures print and exit 1.

#include "notebook.h"
#include "notebookpackage.h"
#include "textdocumentjson.h"
#include "zipstore.h"

#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QtDebug>

static int failures = 0;

static void check(bool condition, const QString& what) {
    if (condition) {
        qInfo("PASS: %s", qPrintable(what));
    } else {
        qWarning("FAIL: %s", qPrintable(what));
        ++failures;
    }
}

// Build a text element JSON the same way TextElement::toJson does, but
// without needing a scene: formatted QTextDocument -> TextDocumentJson.
static QJsonObject makeTextElement(const QString& id, const QString& markedText,
                                   const QString& linkTargetId) {
    QTextDocument document;
    QTextCursor cursor(&document);
    QTextCharFormat bold;
    bold.setFontWeight(QFont::Bold);
    cursor.insertText(QStringLiteral("Session log: "), bold);
    cursor.insertText(markedText);
    if (!linkTargetId.isEmpty()) {
        cursor.insertText(QStringLiteral(" "));
        QTextCharFormat link;
        link.setAnchor(true);
        link.setAnchorHref(QStringLiteral("note://page/%1").arg(linkTargetId));
        link.setForeground(QColor(Qt::blue));
        cursor.insertText(QStringLiteral("see character sheet"), link);
    }
    QJsonObject element;
    element[QStringLiteral("id")] = id;
    element[QStringLiteral("kind")] = QStringLiteral("text");
    element[QStringLiteral("x")] = 40;
    element[QStringLiteral("y")] = 40;
    element[QStringLiteral("w")] = 320;
    element[QStringLiteral("h")] = 120;
    element[QStringLiteral("document")] = TextDocumentJson::serialize(&document);
    return element;
}

static QByteArray makePngBytes() {
    QImage image(24, 24, QImage::Format_RGB32);
    image.fill(QColor(Qt::red));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // --- Build the notebook -------------------------------------------------
    Notebook notebook;
    notebook.title = QStringLiteral("Champions Campaign");

    Section logSection;
    logSection.id = Notebook::newId();
    logSection.title = QStringLiteral("Campaign Log");
    Page sessionPage;
    sessionPage.id = Notebook::newId();
    sessionPage.title = QStringLiteral("Session 12");

    Section charsSection;
    charsSection.id = Notebook::newId();
    charsSection.title = QStringLiteral("Characters");
    Page heroPage;
    heroPage.id = Notebook::newId();
    heroPage.title = QStringLiteral("Hero Sheet");

    // Session page links to the hero page (valid internal link).
    sessionPage.elements.append(makeTextElement(Notebook::newId(),
                                                QStringLiteral("The heroes reached the spire."),
                                                heroPage.id));
    QJsonObject imageElement;
    imageElement[QStringLiteral("id")] = Notebook::newId();
    imageElement[QStringLiteral("kind")] = QStringLiteral("image");
    imageElement[QStringLiteral("x")] = 400;
    imageElement[QStringLiteral("y")] = 40;
    imageElement[QStringLiteral("w")] = 24;
    imageElement[QStringLiteral("h")] = 24;
    imageElement[QStringLiteral("media")] = QStringLiteral("map.png");
    sessionPage.elements.append(imageElement);
    sessionPage.media.insert(QStringLiteral("map.png"), makePngBytes());

    heroPage.elements.append(makeTextElement(Notebook::newId(),
                                             QStringLiteral("STR 60, DEX 18."),
                                             QString()));
    logSection.pages.append(sessionPage);
    charsSection.pages.append(heroPage);
    notebook.sections.append(logSection);
    notebook.sections.append(charsSection);

    // --- Save + integrity ---------------------------------------------------
    QTemporaryDir tempDir;
    check(tempDir.isValid(), "temp dir created");
    const QString filePath = tempDir.filePath(QStringLiteral("champions.qnb"));

    QString error;
    check(NotebookPackage::saveAtomic(notebook, filePath, &error),
          QStringLiteral("saveAtomic: %1").arg(error));
    check(QFile::exists(filePath), "notebook file exists after save");

    const IntegrityReport report = NotebookPackage::integrityCheck(filePath);
    qInfo("integrity: %s", qPrintable(report.summary()));
    check(report.ok, "integrity ok on freshly saved notebook");
    check(report.pagesFound == 2, QStringLiteral("integrity pagesFound == 2 (got %1)").arg(report.pagesFound));
    check(report.mediaFound == 1, QStringLiteral("integrity mediaFound == 1 (got %1)").arg(report.mediaFound));
    check(report.brokenLinks.isEmpty(), "no broken links in valid notebook");

    // Saving again must leave a .bak of the previous version.
    check(NotebookPackage::saveAtomic(notebook, filePath, &error), "second saveAtomic");
    check(QFile::exists(filePath + QStringLiteral(".bak")), ".bak kept after re-save");

    // --- Load back and verify round trip ------------------------------------
    Notebook loaded;
    IntegrityReport loadReport;
    check(NotebookPackage::load(filePath, &loaded, &error, &loadReport),
          QStringLiteral("load: %1").arg(error));
    check(loaded.title == notebook.title, "notebook title round-trips");
    check(loaded.sections.size() == 2, "section count round-trips");
    check(loaded.pageCount() == 2, "page count round-trips");

    const Page* loadedSession = loaded.findPage(sessionPage.id);
    check(loadedSession != nullptr, "session page found by original GUID");
    if (loadedSession != nullptr) {
        check(loadedSession->title == QStringLiteral("Session 12"), "page title round-trips");
        check(loadedSession->elements.size() == 2, "element count round-trips");
        check(loadedSession->media.value(QStringLiteral("map.png")) == sessionPage.media.value(QStringLiteral("map.png")),
              "image media bytes round-trip exactly");
        const QJsonObject docJson = loadedSession->elements.first().value(QStringLiteral("document")).toObject();
        QTextDocument* doc = TextDocumentJson::deserialize(docJson);
        check(doc != nullptr && doc->toPlainText().contains(QStringLiteral("heroes reached the spire")),
              "formatted text round-trips through the serializer");
        check(doc != nullptr && doc->toPlainText().contains(QStringLiteral("Session log:")),
              "bold lead-in text present after round trip");
        delete doc;
    }

    // --- Broken-link detection ----------------------------------------------
    Notebook damaged = notebook;
    Page* damagedSession = damaged.findPage(sessionPage.id);
    const QString ghostId = Notebook::newId();
    damagedSession->elements.append(makeTextElement(Notebook::newId(),
                                                    QStringLiteral("A link to a deleted page."),
                                                    ghostId));
    const QString damagedPath = tempDir.filePath(QStringLiteral("damaged.qnb"));
    check(NotebookPackage::saveAtomic(damaged, damagedPath, &error), "damaged copy saves");
    const IntegrityReport damagedReport = NotebookPackage::integrityCheck(damagedPath);
    qInfo("damaged integrity: %s", qPrintable(damagedReport.summary()));
    check(damagedReport.brokenLinks.contains(ghostId),
          "broken internal link is detected and reported");

    // --- Index-rebuild fallback ---------------------------------------------
    // A .qnb with NO notebook.json at all: load() must rebuild the notebook
    // structure by scanning <Section>/<Page>.json entries.
    {
        QJsonArray elements;
        elements.append(makeTextElement(Notebook::newId(), QStringLiteral("Recovered text."), QString()));
        QJsonObject pageDoc;
        pageDoc[QStringLiteral("format")] = QStringLiteral("qtnote-page");
        pageDoc[QStringLiteral("version")] = 1;
        pageDoc[QStringLiteral("pageId")] = Notebook::newId();
        pageDoc[QStringLiteral("title")] = QStringLiteral("Recovered Page");
        pageDoc[QStringLiteral("elements")] = elements;
        QMap<QString, QByteArray> entries;
        entries.insert(QStringLiteral("Solo Section/Recovered Page.json"),
                       QJsonDocument(pageDoc).toJson(QJsonDocument::Indented));
        const QString orphanPath = tempDir.filePath(QStringLiteral("noindex.qnb"));
        check(ZipStore::writeFile(orphanPath, entries, &error), "index-less file written");
        Notebook recovered;
        IntegrityReport recoveredReport;
        check(NotebookPackage::load(orphanPath, &recovered, &error, &recoveredReport),
              QStringLiteral("index-less load: %1").arg(error));
        check(recoveredReport.rebuiltIndex, "rebuild-from-scan was used and reported");
        check(recovered.pageCount() == 1, "rebuilt notebook has the scanned page");
        check(recovered.sections.size() == 1
                  && recovered.sections.first().title == QStringLiteral("Solo Section"),
              "rebuilt section takes its title from the folder name");
    }

    if (failures == 0) {
        qInfo("SMOKE PASS");
        return 0;
    }
    qWarning("SMOKE FAIL (%d failures)", failures);
    return 1;
}
