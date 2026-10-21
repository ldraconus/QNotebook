#include "mainwindow.h"

#include "pagecanvas.h"
#include "pdfexplode.h"

#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPdfWriter>
#include <QPainter>
#include <QSplitter>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QTreeWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("QtNotebook"));
    resize(1200, 800);

    mTree = new QTreeWidget(this);
    mTree->setHeaderLabel(QStringLiteral("Notebook"));
    mTree->setMinimumWidth(240);
    mCanvas = new PageCanvas(this);

    QSplitter* splitter = new QSplitter(this);
    splitter->addWidget(mTree);
    splitter->addWidget(mCanvas);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    QToolBar* navBar = addToolBar(QStringLiteral("Navigation"));
    mBackAction = navBar->addAction(QStringLiteral("< Back"), this, &MainWindow::goBack);
    mForwardAction = navBar->addAction(QStringLiteral("Forward >"), this, &MainWindow::goForward);

    buildMenus();
    connect(mTree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* item, int) { onTreeItemActivated(item); });

    makeDefaultNotebook();
    rebuildTree();
    statusBar()->showMessage(QStringLiteral("New notebook — not saved yet"));
    updateHistoryButtons();
}

void MainWindow::buildMenus() {
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("&New Notebook"), this, &MainWindow::newNotebook);
    fileMenu->addAction(QStringLiteral("&Open..."), this, &MainWindow::openNotebook);
    fileMenu->addAction(QStringLiteral("&Save"), this, &MainWindow::saveNotebook);
    fileMenu->addAction(QStringLiteral("Save &As..."), this, &MainWindow::saveNotebookAs);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("&Export Page as PDF..."), this, &MainWindow::exportPdf);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Add &Section..."), this, &MainWindow::addSection);
    fileMenu->addAction(QStringLiteral("Add &Page..."), this, &MainWindow::addPage);

    // Search is a v1 spec item; the full-text UI lands after the scaffold.
    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    QAction* searchAction = editMenu->addAction(QStringLiteral("&Search..."));
    searchAction->setEnabled(false);   // TODO(search): full-text search UI per spec v1

    QMenu* insertMenu = menuBar()->addMenu(QStringLiteral("&Insert"));
    insertMenu->addAction(QStringLiteral("&Text Box"), this, &MainWindow::insertTextBox);
    insertMenu->addAction(QStringLiteral("&Image..."), this, &MainWindow::insertImage);
    insertMenu->addAction(QStringLiteral("&PDF (explode into pages)..."), this, &MainWindow::insertPdf);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(QStringLiteral("Zoom &In"), this, [this]() { mCanvas->zoomIn(); });
    viewMenu->addAction(QStringLiteral("Zoom &Out"), this, [this]() { mCanvas->zoomOut(); });
    viewMenu->addAction(QStringLiteral("&Reset Zoom"), this, [this]() { mCanvas->zoomReset(); });
}

void MainWindow::makeDefaultNotebook() {
    mNotebook = Notebook();
    mNotebook.title = QStringLiteral("Untitled Notebook");
    Section section;
    section.id = Notebook::newId();
    section.title = QStringLiteral("Quick Notes");
    Page page;
    page.id = Notebook::newId();
    page.title = QStringLiteral("Untitled Page");
    section.pages.append(page);
    mNotebook.sections.append(section);
    mCurrentPageId = page.id;
    mHistory = { page.id };
    mHistoryIndex = 0;
}

void MainWindow::rebuildTree() {
    mTree->clear();
    QTreeWidgetItem* root = new QTreeWidgetItem(mTree, QStringList{ mNotebook.title });
    for (const Section& section : mNotebook.sections) {
        QTreeWidgetItem* sectionItem = new QTreeWidgetItem(root, QStringList{ section.title });
        sectionItem->setData(0, Qt::UserRole, section.id);
        sectionItem->setData(0, Qt::UserRole + 1, QStringLiteral("section"));
        for (const Page& page : section.pages) {
            QTreeWidgetItem* pageItem = new QTreeWidgetItem(sectionItem, QStringList{ page.title });
            pageItem->setData(0, Qt::UserRole, page.id);
            pageItem->setData(0, Qt::UserRole + 1, QStringLiteral("page"));
            if (page.id == mCurrentPageId) {
                mTree->setCurrentItem(pageItem);
                pageItem->setExpanded(true);
            }
        }
        sectionItem->setExpanded(true);
    }
    root->setExpanded(true);
    Page* current = mNotebook.findPage(mCurrentPageId);
    if (current != nullptr)
        mCanvas->loadPage(*current);
}

void MainWindow::syncCanvasToCurrentPage() {
    Page* current = mNotebook.findPage(mCurrentPageId);
    if (current != nullptr)
        mCanvas->collectInto(current);
}

void MainWindow::showPage(const QString& pageId, bool pushHistory) {
    if (pageId == mCurrentPageId && !pushHistory)
        return;
    syncCanvasToCurrentPage();
    Page* page = mNotebook.findPage(pageId);
    if (page == nullptr)
        return;
    mCurrentPageId = pageId;
    mCanvas->loadPage(*page);
    if (pushHistory) {
        // Truncate any forward history, browser-style.
        while (mHistory.size() > mHistoryIndex + 1)
            mHistory.removeLast();
        mHistory.append(pageId);
        mHistoryIndex = mHistory.size() - 1;
    }
    updateHistoryButtons();
    // Keep the tree selection in step without rebuilding (which reloads canvas).
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* root = mTree->topLevelItem(i);
        for (int s = 0; s < root->childCount(); ++s) {
            QTreeWidgetItem* sectionItem = root->child(s);
            for (int p = 0; p < sectionItem->childCount(); ++p) {
                QTreeWidgetItem* pageItem = sectionItem->child(p);
                if (pageItem->data(0, Qt::UserRole).toString() == pageId)
                    mTree->setCurrentItem(pageItem);
            }
        }
    }
}

void MainWindow::updateHistoryButtons() {
    mBackAction->setEnabled(mHistoryIndex > 0);
    mForwardAction->setEnabled(mHistoryIndex >= 0 && mHistoryIndex < mHistory.size() - 1);
}

void MainWindow::goBack() {
    if (mHistoryIndex <= 0)
        return;
    --mHistoryIndex;
    const QString target = mHistory.at(mHistoryIndex);
    syncCanvasToCurrentPage();           // save canvas edits before leaving the page
    mCurrentPageId = QString();          // force showPage to load (its own sync is then a no-op)
    showPage(target, false);
    updateHistoryButtons();
}

void MainWindow::goForward() {
    if (mHistoryIndex < 0 || mHistoryIndex >= mHistory.size() - 1)
        return;
    ++mHistoryIndex;
    const QString target = mHistory.at(mHistoryIndex);
    syncCanvasToCurrentPage();
    mCurrentPageId = QString();
    showPage(target, false);
    updateHistoryButtons();
}

void MainWindow::onTreeItemActivated(QTreeWidgetItem* item) {
    if (item == nullptr)
        return;
    if (item->data(0, Qt::UserRole + 1).toString() != QStringLiteral("page"))
        return;
    const QString pageId = item->data(0, Qt::UserRole).toString();
    if (pageId != mCurrentPageId)
        showPage(pageId, true);
}

void MainWindow::updateHealthStatus(const IntegrityReport& report) {
    statusBar()->showMessage(QStringLiteral("Notebook health: %1").arg(report.summary()));
}

void MainWindow::openNotebookFile(const QString& filePath) {
    Notebook loaded;
    QString error;
    IntegrityReport report;
    if (!NotebookPackage::load(filePath, &loaded, &error, &report)) {
        QMessageBox::warning(this, QStringLiteral("Open failed"),
                             QStringLiteral("Could not open %1:\n%2\n\nStarting with a new notebook instead.")
                                 .arg(filePath, error));
        return;
    }
    mNotebook = loaded;
    mFilePath = filePath;
    setWindowTitle(QStringLiteral("QtNotebook — %1").arg(QFileInfo(filePath).fileName()));
    mHistory.clear();
    mHistoryIndex = -1;
    mCurrentPageId = QString();
    if (mNotebook.pageCount() > 0) {
        const Page& first = mNotebook.sections.first().pages.first();
        mCurrentPageId = first.id;
        mHistory = { first.id };
        mHistoryIndex = 0;
    }
    rebuildTree();
    updateHistoryButtons();
    updateHealthStatus(report);
}

void MainWindow::newNotebook() {
    syncCanvasToCurrentPage();
    makeDefaultNotebook();
    mFilePath.clear();
    setWindowTitle(QStringLiteral("QtNotebook"));
    rebuildTree();
    updateHistoryButtons();
    statusBar()->showMessage(QStringLiteral("New notebook — not saved yet"));
}

void MainWindow::openNotebook() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open Notebook"), QString(),
        QStringLiteral("QtNotebook files (*.qnb);;All files (*)"));
    if (!path.isEmpty())
        openNotebookFile(path);
}

void MainWindow::saveNotebook() {
    if (mFilePath.isEmpty()) {
        saveNotebookAs();
        return;
    }
    syncCanvasToCurrentPage();
    QString error;
    if (!NotebookPackage::saveAtomic(mNotebook, mFilePath, &error)) {
        // Loud, never silent: the original file is untouched on failure.
        QMessageBox::critical(this, QStringLiteral("Save FAILED"),
                              QStringLiteral("Your notebook was NOT saved:\n%1\n\n"
                                             "The previous saved copy (if any) is untouched.")
                                  .arg(error));
        return;
    }
    updateHealthStatus(NotebookPackage::integrityCheck(mFilePath));
}

void MainWindow::saveNotebookAs() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Notebook As"), QStringLiteral("notebook.qnb"),
        QStringLiteral("QtNotebook files (*.qnb);;All files (*)"));
    if (path.isEmpty())
        return;
    mFilePath = path;
    setWindowTitle(QStringLiteral("QtNotebook — %1").arg(QFileInfo(path).fileName()));
    saveNotebook();
}

void MainWindow::exportPdf() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Page as PDF"), QStringLiteral("page.pdf"),
        QStringLiteral("PDF files (*.pdf)"));
    if (path.isEmpty())
        return;
    syncCanvasToCurrentPage();
    QPdfWriter writer(path);
    writer.setResolution(96);
    QPainter painter(&writer);
    // Scaffold-level export: render the whole (expanded) scene, paginated
    // by QPdfWriter's page size only in the sense of one big first cut —
    // TODO(export): proper multi-page pagination of the expanding canvas.
    mCanvas->scene()->render(&painter);
    painter.end();
    statusBar()->showMessage(QStringLiteral("Exported current page to %1").arg(path));
}

void MainWindow::addSection() {
    bool ok = false;
    const QString title = QInputDialog::getText(this, QStringLiteral("Add Section"),
                                                QStringLiteral("Section title:"), QLineEdit::Normal,
                                                QStringLiteral("New Section"), &ok);
    if (!ok || title.trimmed().isEmpty())
        return;
    Section section;
    section.id = Notebook::newId();
    section.title = title.trimmed();
    mNotebook.sections.append(section);
    rebuildTree();
}

void MainWindow::addPage() {
    if (mNotebook.sections.isEmpty())
        addSection();
    if (mNotebook.sections.isEmpty())
        return;
    bool ok = false;
    const QString title = QInputDialog::getText(this, QStringLiteral("Add Page"),
                                                QStringLiteral("Page title:"), QLineEdit::Normal,
                                                QStringLiteral("New Page"), &ok);
    if (!ok || title.trimmed().isEmpty())
        return;
    syncCanvasToCurrentPage();
    Page page;
    page.id = Notebook::newId();
    page.title = title.trimmed();
    mNotebook.sections.first().pages.append(page);
    rebuildTree();
    showPage(page.id, true);
}

void MainWindow::insertTextBox() {
    const QPointF center = mCanvas->mapToScene(mCanvas->viewport()->rect().center());
    mCanvas->addTextElement(center, QStringLiteral("New text"));
}

void MainWindow::insertImage() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Insert Image"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.gif *.bmp)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("Insert Image"),
                             QStringLiteral("Could not read %1").arg(path));
        return;
    }
    const QByteArray data = file.readAll();
    const QString mediaName = QFileInfo(path).fileName();
    const QPointF center = mCanvas->mapToScene(mCanvas->viewport()->rect().center());
    mCanvas->addImageElement(center, mediaName, data);
    // Record the bytes on the current page now, so Save works even if the
    // pixmap round-trip in collectInto would re-encode them anyway.
    Page* current = mNotebook.findPage(mCurrentPageId);
    if (current != nullptr)
        current->media.insert(mediaName, data);
}

void MainWindow::insertPdf() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Insert PDF (explode into pages)"), QString(),
        QStringLiteral("PDF files (*.pdf)"));
    if (path.isEmpty())
        return;
    if (!PdfExplode::isAvailable()) {
        // Surface the stub honestly instead of failing silently.
        const PdfExplode::Result result = PdfExplode::explodePdf(path, QString());
        QMessageBox::information(this, QStringLiteral("PDF Import Unavailable"), result.error);
        return;
    }
    QTemporaryDir tempDir;
    const PdfExplode::Result result = PdfExplode::explodePdf(path, tempDir.path());
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("PDF Import Failed"), result.error);
        return;
    }
    if (!result.warning.isEmpty()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, QStringLiteral("Large PDF"), result.warning + QStringLiteral("\n\nExplode anyway?"));
        if (answer != QMessageBox::Yes)
            return;
    }
    // TODO(original-pdf): also store the source PDF on the current page as
    // "_original.pdf" in its media map, so the package keeps it invisibly.
    Page* current = mNotebook.findPage(mCurrentPageId);
    QPointF pos = mCanvas->mapToScene(mCanvas->viewport()->rect().topLeft()) + QPointF(40, 40);
    for (const QString& pngPath : result.pngPaths) {
        QFile pngFile(pngPath);
        if (!pngFile.open(QIODevice::ReadOnly))
            continue;
        const QByteArray data = pngFile.readAll();
        const QString mediaName = QFileInfo(pngPath).fileName();
        if (current != nullptr)
            current->media.insert(mediaName, data);
        mCanvas->addImageElement(pos, mediaName, data);
        pos += QPointF(0, 320);   // stack pages one after the other
    }
}
