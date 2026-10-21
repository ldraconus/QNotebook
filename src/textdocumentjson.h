#pragma once

// textdocumentjson.h — QTextDocument <-> JSON, lossless where Qt allows.
//
// Motivation: QTextDocument's built-in I/O (HTML import/export, ODF) is lossy.
// This module walks the document model directly — blocks, frames, tables,
// lists, images — and persists every format property, so a save/load round
// trip preserves the document.
//
// Resources (images): by default they are inlined as base64 PNG in the JSON
// under "resources". Pass a custom ResourceWriter/ResourceReader to store
// them elsewhere (e.g. as separate entries in your zip package).
//
// How completeness is guaranteed: formats are serialized as their full
// property map (QTextFormat::properties()). Known properties get readable
// names ("fontFamily"); anything else is kept as "prop_<id>" with a tagged
// value, so no property is ever silently dropped.
//
// Target: Qt 6. (Qt 5.15 mostly works; QVariant::metaType() needs swapping
// for QVariant::type().)
//
// Qt 6.10 vs 6.11: builds on any Qt 6. The 6.11-only properties
// (FontFeatures, FontVariableAxes) and a few older versioned additions are
// behind QT_VERSION guards in the .cpp; older Qt simply never sees them.
//
// NOTE: carefully reviewed but not compile-tested on this machine (no Qt
// here) — build it and send me any errors; I'll fix them fast.

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QTextDocument>
#include <QTextFormat>
#include <functional>

class TextDocumentJson
{
public:
    // Called for every image/resource found while serializing.
    // Return the JSON object to store under resources[id].
    // Default: { "mime": ..., "data": "<base64>" } inline.
    // For zip storage return e.g. { "mime": ..., "zipPath": "media/img1.png" }
    // and write `data` into the zip yourself.
    using ResourceWriter = std::function<QJsonObject(const QString &id,
                                                     const QString &mimeType,
                                                     const QByteArray &data)>;

    // Reverse of the above: given the stored JSON object, return raw bytes.
    using ResourceReader = std::function<QByteArray(const QString &id,
                                                    const QJsonObject &stored)>;

    static QJsonObject serialize(const QTextDocument *doc,
                                 ResourceWriter writer = defaultResourceWriter());

    // Caller takes ownership of the returned document.
    static QTextDocument *deserialize(const QJsonObject &obj,
                                      ResourceReader reader = defaultResourceReader());

    static ResourceWriter defaultResourceWriter();
    static ResourceReader defaultResourceReader();

    // Format <-> JSON helpers (also handy for debugging).
    static QJsonObject formatToJson(const QTextFormat &format);

    template <typename F>
    static F jsonToFormat(const QJsonObject &obj)
    {
        F f;
        const QJsonObject props = obj.value(QLatin1String("properties")).toObject();
        for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
            const int id = propertyId(it.key());
            if (id >= 0)
                f.setProperty(id, jsonToVariant(it.value().toObject()));
        }
        return f;
    }

private:
    static QString propertyName(int id);
    static int propertyId(const QString &name);
    static QJsonObject variantToJson(const QVariant &value);
    static QVariant jsonToVariant(const QJsonObject &obj);
    static QVariant jsonToVariant(const QJsonValue &val);
};
