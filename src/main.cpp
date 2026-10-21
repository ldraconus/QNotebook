#include "mainwindow.h"

#include <QApplication>
#include <QStringList>

// Usage: qtnotebook [notebook.qnb]
// A file argument (from the future double-click file association for the
// .qnb extension — still a placeholder name) opens that notebook; with no
// argument the app starts on a new untitled notebook.
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("QtNotebook"));
    app.setOrganizationName(QStringLiteral("QtNotebook"));

    MainWindow window;
    const QStringList args = app.arguments();
    if (args.size() > 1)
        window.openNotebookFile(args.at(1));
    window.show();
    return app.exec();
}
