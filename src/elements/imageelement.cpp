#include "imageelement.h"

#include "../notebook.h"

#include <QPixmap>

ImageElement::ImageElement() {
    mId = Notebook::newId();
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable
             | QGraphicsItem::ItemSendsGeometryChanges);
    setTransformationMode(Qt::SmoothTransformation);
}

bool ImageElement::setImageData(const QByteArray& data) {
    QPixmap pixmap;
    if (!pixmap.loadFromData(data))
        return false;
    setPixmap(pixmap);
    return true;
}

QJsonObject ImageElement::toJson() const {
    QJsonObject json;
    json[QStringLiteral("id")] = mId;
    json[QStringLiteral("kind")] = QStringLiteral("image");
    json[QStringLiteral("x")] = pos().x();
    json[QStringLiteral("y")] = pos().y();
    json[QStringLiteral("w")] = pixmap().width() * scale();
    json[QStringLiteral("h")] = pixmap().height() * scale();
    json[QStringLiteral("media")] = mMediaFileName;
    return json;
}

ImageElement* ImageElement::fromJson(const QJsonObject& json, const QByteArray& mediaData) {
    ImageElement* element = new ImageElement;
    element->mId = json.value(QStringLiteral("id")).toString(element->mId);
    element->mMediaFileName = json.value(QStringLiteral("media")).toString();
    if (!mediaData.isEmpty())
        element->setImageData(mediaData);
    // Scale the native pixmap to the stored display size.
    const qreal width = json.value(QStringLiteral("w")).toDouble();
    if (width > 0 && !element->pixmap().isNull() && element->pixmap().width() > 0)
        element->setScale(width / element->pixmap().width());
    element->setPos(json.value(QStringLiteral("x")).toDouble(),
                    json.value(QStringLiteral("y")).toDouble());
    return element;
}
