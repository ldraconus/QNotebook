#pragma once

// notebook.h — in-memory notebook model.
//
// Notebook > Section (tab) > Page. A page's content is a list of element
// JSON objects (see elements/) plus the page's media bytes (images and
// exploded PDF pages), keyed by file name inside the page's .media folder.
// Keeping content as element JSON in the model keeps NotebookPackage free of
// any QGraphicsScene dependency, which is what lets the smoke test drive the
// whole save/load path headlessly.

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>

struct Page
{
    QString id;                                  // QUuid string, stable across rename/move — links target this
    QString title;
    QList<QJsonObject> elements;                 // canvas elements, in z-order
    QMap<QString, QByteArray> media;             // file name -> bytes, stored under <Page>.media/
};

struct Section
{
    QString id;
    QString title;
    QList<Page> pages;
};

struct Notebook
{
    QString title = QStringLiteral("Untitled Notebook");
    QList<Section> sections;

    static QString newId();
    Page* findPage(const QString& pageId);
    const Page* findPage(const QString& pageId) const;
    Section* findSection(const QString& sectionId);
    int pageCount() const;
};
