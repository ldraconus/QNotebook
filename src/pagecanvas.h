#pragma once

// pagecanvas.h — the page canvas: a QGraphicsView over an expanding scene.
//
// No fixed page size: the scene rect grows to fit the elements' bounding
// rect plus a margin whenever an element is added, moved, or edited.
// Clicking empty canvas creates a new text element in edit mode (OneNote-style
// click-anywhere-to-type). Print/export paginates the scene after the fact.

#include "notebook.h"

#include <QGraphicsView>
#include <QJsonObject>
#include <QList>

class QGraphicsScene;

class PageCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    explicit PageCanvas(QWidget* parent = nullptr);

    // Load a page's elements into the scene. Media bytes come from page.media.
    void loadPage(const Page& page);
    // Collect the scene back into element JSON + media (for saving / switching pages).
    void collectInto(Page* page) const;

    void addTextElement(const QPointF& scenePos, const QString& text = QString());
    void addImageElement(const QPointF& scenePos, const QString& mediaFileName, const QByteArray& data);

    void zoomIn();
    void zoomOut();
    void zoomReset();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void expandSceneToFit();
    bool mExpanding = false;
    QGraphicsScene* mScene = nullptr;
};
