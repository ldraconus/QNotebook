#pragma once

// imageelement.h — an image on the page canvas, separate from text.
//
// Per the Sept 2026 decision, images are NOT embedded in a QTextDocument;
// they live alongside text elements at canvas level so they can overlap,
// move, and resize independently. The image bytes live in the page's media
// map (Notebook model) under mediaFileName(); this item only keeps the
// pixmap for display plus the file name for serialization. Exploded PDF
// pages arrive here too — after import they are ordinary image elements.

#include <QByteArray>
#include <QGraphicsPixmapItem>
#include <QJsonObject>
#include <QString>

class ImageElement : public QGraphicsPixmapItem
{
public:
    ImageElement();

    QString elementId() const { return mId; }
    void setElementId(const QString& id) { mId = id; }
    QString mediaFileName() const { return mMediaFileName; }
    void setMediaFileName(const QString& name) { mMediaFileName = name; }

    // Load display pixmap from raw bytes (PNG/JPEG/... as stored in media).
    bool setImageData(const QByteArray& data);

    QJsonObject toJson() const;
    // mediaData is the bytes for the element's media file (may be empty in
    // tests that only need geometry; the pixmap is then left blank).
    static ImageElement* fromJson(const QJsonObject& json, const QByteArray& mediaData);

private:
    QString mId;
    QString mMediaFileName;
};
