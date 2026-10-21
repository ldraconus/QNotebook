#pragma once

// textelement.h — a rich-text box on the page canvas.
//
// Holds its content in a QTextDocument (via QGraphicsTextItem) with a
// transparent background, so text boxes can overlap images and each other.
// Serialization goes through TextDocumentJson (copied from ~/workspace/qtnotes/),
// never through HTML, so formatting round-trips losslessly.
// Internal links are just anchors with href "note://page/<page-guid>";
// external links are ordinary https anchors in the same text.

#include <QGraphicsTextItem>
#include <QJsonObject>
#include <QString>

class TextElement : public QGraphicsTextItem
{
public:
    explicit TextElement(const QString& plainText = QString());

    QString elementId() const { return mId; }
    void setElementId(const QString& id) { mId = id; }

    QJsonObject toJson() const;
    static TextElement* fromJson(const QJsonObject& json);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QString mId;
};
