#include "textelement.h"

#include "../notebook.h"
#include "../textdocumentjson.h"

#include <QTextDocument>

TextElement::TextElement(const QString& plainText) {
    mId = Notebook::newId();
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable
             | QGraphicsItem::ItemSendsGeometryChanges);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setTextWidth(320);
    if (!plainText.isEmpty())
        setPlainText(plainText);
    // No background fill is drawn by QGraphicsTextItem by default: the
    // document shows through transparently onto the canvas, as planned.
}

QJsonObject TextElement::toJson() const {
    QJsonObject json;
    json[QStringLiteral("id")] = mId;
    json[QStringLiteral("kind")] = QStringLiteral("text");
    json[QStringLiteral("x")] = pos().x();
    json[QStringLiteral("y")] = pos().y();
    json[QStringLiteral("w")] = boundingRect().width();
    json[QStringLiteral("h")] = boundingRect().height();
    json[QStringLiteral("document")] = TextDocumentJson::serialize(document());
    return json;
}

TextElement* TextElement::fromJson(const QJsonObject& json) {
    TextElement* element = new TextElement;
    element->mId = json.value(QStringLiteral("id")).toString(element->mId);
    const qreal width = json.value(QStringLiteral("w")).toDouble(320);
    element->setTextWidth(width);
    const QJsonObject docJson = json.value(QStringLiteral("document")).toObject();
    if (!docJson.isEmpty()) {
        QTextDocument* doc = TextDocumentJson::deserialize(docJson);
        if (doc != nullptr) {
            doc->setTextWidth(width);
            element->setDocument(doc);   // QGraphicsTextItem takes ownership
        }
    }
    element->setPos(json.value(QStringLiteral("x")).toDouble(),
                    json.value(QStringLiteral("y")).toDouble());
    return element;
}

QVariant TextElement::itemChange(GraphicsItemChange change, const QVariant& value) {
    // TODO(links): intercept anchor clicks for note://page/<guid> here /
    // in mouseReleaseEvent and emit a navigation request to MainWindow.
    // For the scaffold, anchors are inert in the canvas.
    return QGraphicsTextItem::itemChange(change, value);
}
