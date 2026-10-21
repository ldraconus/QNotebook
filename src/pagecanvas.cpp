#include "pagecanvas.h"

#include "elements/imageelement.h"
#include "elements/textelement.h"

#include <QBuffer>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QScrollBar>

PageCanvas::PageCanvas(QWidget* parent) : QGraphicsView(parent) {
    mScene = new QGraphicsScene(this);
    mScene->setSceneRect(0, 0, 1024, 768);
    setScene(mScene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::RubberBandDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Scene changes (an item moved, text grew, ...) grow the scene rect.
    connect(mScene, &QGraphicsScene::changed, this, [this](const QList<QRectF>&) { expandSceneToFit(); });
}

void PageCanvas::loadPage(const Page& page) {
    mScene->clear();
    mScene->setSceneRect(0, 0, 1024, 768);
    for (const QJsonObject& elementJson : page.elements) {
        const QString kind = elementJson.value(QStringLiteral("kind")).toString();
        if (kind == QStringLiteral("text")) {
            mScene->addItem(TextElement::fromJson(elementJson));
        } else if (kind == QStringLiteral("image")) {
            const QString media = elementJson.value(QStringLiteral("media")).toString();
            mScene->addItem(ImageElement::fromJson(elementJson, page.media.value(media)));
        }
    }
    expandSceneToFit();
}

void PageCanvas::collectInto(Page* page) const {
    page->elements.clear();
    // Keep existing media bytes; add any media referenced by image items.
    QList<QGraphicsItem*> items = mScene->items(Qt::AscendingOrder);
    for (QGraphicsItem* item : items) {
        if (TextElement* text = dynamic_cast<TextElement*>(item)) {
            page->elements.append(text->toJson());
        } else if (ImageElement* image = dynamic_cast<ImageElement*>(item)) {
            page->elements.append(image->toJson());
            if (!page->media.contains(image->mediaFileName())) {
                // Display pixmap back to PNG bytes so a freshly inserted
                // image survives a save even before any other bookkeeping.
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                image->pixmap().save(&buffer, "PNG");
                page->media.insert(image->mediaFileName(), bytes);
            }
        }
    }
}

void PageCanvas::addTextElement(const QPointF& scenePos, const QString& text) {
    TextElement* element = new TextElement(text);
    element->setPos(scenePos);
    mScene->addItem(element);
    element->setFocus();
    expandSceneToFit();
}

void PageCanvas::addImageElement(const QPointF& scenePos, const QString& mediaFileName, const QByteArray& data) {
    ImageElement* element = new ImageElement;
    element->setMediaFileName(mediaFileName);
    element->setImageData(data);
    element->setPos(scenePos);
    mScene->addItem(element);
    expandSceneToFit();
}

void PageCanvas::zoomIn() {
    scale(1.25, 1.25);
}

void PageCanvas::zoomOut() {
    scale(0.8, 0.8);
}

void PageCanvas::zoomReset() {
    resetTransform();
}

void PageCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        QGraphicsItem* hit = mScene->itemAt(scenePos, transform());
        if (hit == nullptr) {
            // Click anywhere empty to start typing — the OneNote gesture.
            // If a text element is already being edited, let the base class
            // handle the click (it moves the caret / ends the edit).
            bool editing = false;
            for (QGraphicsItem* item : mScene->items()) {
                if (TextElement* text = dynamic_cast<TextElement*>(item)) {
                    if (text->hasFocus())
                        editing = true;
                }
            }
            if (!editing) {
                addTextElement(scenePos);
                event->accept();
                return;
            }
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void PageCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    // Double-click always starts a new text element on empty canvas,
    // even if another element currently has focus.
    if (event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        if (mScene->itemAt(scenePos, transform()) == nullptr) {
            addTextElement(scenePos);
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void PageCanvas::expandSceneToFit() {
    if (mExpanding || mScene->items().isEmpty())
        return;
    mExpanding = true;
    const qreal margin = 200;
    QRectF wanted = mScene->itemsBoundingRect().adjusted(-margin, -margin, margin, margin);
    wanted = wanted.united(QRectF(0, 0, 1024, 768));
    if (wanted != mScene->sceneRect())
        mScene->setSceneRect(wanted);
    mExpanding = false;
}
