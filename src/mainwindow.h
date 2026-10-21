#pragma once

// mainwindow.h — 3-pane main window (tree | canvas; no right pane in scaffold).
//
// NOTE on dialogs: QFileDialog static helpers are used in this scaffold for
// brevity. They spin a modal loop in some platform plugins, which is fine on
// desktop v1; the Android/WASM pass replaces core flows (Open/Save/Insert)
// with async, callback-style dialogs per the user's standing convention —
// do not add nested QDialog::exec() flows in the meantime.

#include "notebook.h"
#include "notebookpackage.h"

#include <QMainWindow>
#include <QStringList>

class PageCanvas;
class QTreeWidget;
class QTreeWidgetItem;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Open a notebook file (from argv[1] / file association). On failure,
    // reports loudly and falls back to a new untitled notebook.
    void openNotebookFile(const QString& filePath);

private:
    void buildMenus();
    void rebuildTree();
    void syncCanvasToCurrentPage();
    void showPage(const QString& pageId, bool pushHistory);
    void updateHistoryButtons();
    void updateHealthStatus(const IntegrityReport& report);
    void makeDefaultNotebook();

    void newNotebook();
    void openNotebook();
    void saveNotebook();
    void saveNotebookAs();
    void exportPdf();
    void addSection();
    void addPage();
    void insertTextBox();
    void insertImage();
    void insertPdf();
    void goBack();
    void goForward();
    void onTreeItemActivated(QTreeWidgetItem* item);

    Notebook mNotebook;
    QString mFilePath;                 // empty = untitled, never saved
    QString mCurrentPageId;
    QStringList mHistory;              // page ids, browser-style
    int mHistoryIndex = -1;

    QTreeWidget* mTree = nullptr;
    PageCanvas* mCanvas = nullptr;
    QAction* mBackAction = nullptr;
    QAction* mForwardAction = nullptr;
};
