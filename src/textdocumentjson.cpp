// textdocumentjson.cpp — see header for the design notes.

#include "textdocumentjson.h"

#include <QBuffer>
#include <QDebug>
#include <QImage>
#include <QJsonArray>
#include <QMap>
#include <QPixmap>
#include <QtGlobal>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFrame>
#include <QTextImageFormat>
#include <QTextLength>
#include <QTextList>
#include <QTextOption>
#include <QTextTable>
#include <QUrl>

// ---------------------------------------------------------------------------
// Property id <-> readable name table.
// Uses the real QTextFormat:: enum constants, so a typo fails at compile
// time instead of silently. Anything not listed falls back to "prop_<id>".
// ---------------------------------------------------------------------------

struct PropName { int id; const char *name; };

#define PN(x, n) { int(QTextFormat::x), n }

const PropName kPropNames[] = {
    PN(ObjectIndex, "objectIndex"),
    PN(CssFloat, "cssFloat"),
    PN(LayoutDirection, "layoutDirection"),
    PN(OutlinePen, "outlinePen"),
    // --- character format ---
    PN(FontFamily, "fontFamily"), // deprecated upstream; use fontFamilies
    PN(FontFamilies, "fontFamilies"),
    PN(FontStyleName, "fontStyleName"),
    PN(FontPointSize, "fontPointSize"),
    PN(FontPixelSize, "fontPixelSize"),
    PN(FontSizeAdjustment, "fontSizeAdjustment"),
    PN(FontWeight, "fontWeight"),
    PN(FontItalic, "fontItalic"),
    PN(FontUnderline, "fontUnderline"), // deprecated upstream; use underlineStyle
    PN(FontOverline, "fontOverline"),
    PN(FontStrikeOut, "fontStrikeOut"),
    PN(FontFixedPitch, "fontFixedPitch"),
    PN(FontCapitalization, "fontCapitalization"),
    PN(FontLetterSpacingType, "fontLetterSpacingType"),
    PN(FontLetterSpacing, "fontLetterSpacing"),
    PN(FontWordSpacing, "fontWordSpacing"),
    PN(FontStretch, "fontStretch"),
    PN(FontStyleHint, "fontStyleHint"),
    PN(FontStyleStrategy, "fontStyleStrategy"),
    PN(FontKerning, "fontKerning"),
    PN(FontHintingPreference, "fontHintingPreference"),
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
    PN(FontFeatures, "fontFeatures"),
    PN(FontVariableAxes, "fontVariableAxes"),
#endif
    PN(ForegroundBrush, "foreground"),
    PN(BackgroundBrush, "background"),
    PN(BackgroundImageUrl, "backgroundImageUrl"),
    PN(TextUnderlineColor, "underlineColor"),
    PN(TextUnderlineStyle, "underlineStyle"),
    PN(TextVerticalAlignment, "verticalAlignment"),
    PN(TextOutline, "textOutline"),
    PN(TextToolTip, "toolTip"),
    PN(TextSuperScriptBaseline, "superScriptBaseline"),
    PN(TextSubScriptBaseline, "subScriptBaseline"),
    PN(TextBaselineOffset, "baselineOffset"),
    PN(IsAnchor, "isAnchor"),
    PN(AnchorHref, "anchorHref"),
    PN(AnchorName, "anchorName"),
    PN(ObjectType, "objectType"),
    PN(FullWidthSelection, "fullWidthSelection"),
    PN(ImageName, "imageName"),
    PN(ImageTitle, "imageTitle"),
    PN(ImageAltText, "imageAltText"),
    PN(ImageWidth, "imageWidth"),
    PN(ImageHeight, "imageHeight"),
    PN(ImageQuality, "imageQuality"),
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    PN(ImageMaxWidth, "imageMaxWidth"),
#endif
    // --- block format ---
    PN(BlockAlignment, "alignment"),
    PN(BlockTopMargin, "topMargin"),
    PN(BlockBottomMargin, "bottomMargin"),
    PN(BlockLeftMargin, "leftMargin"),
    PN(BlockRightMargin, "rightMargin"),
    PN(TextIndent, "firstLineIndent"),
    PN(BlockIndent, "indent"),
    PN(BlockNonBreakableLines, "nonBreakableLines"),
    PN(LineHeight, "lineHeight"),
    PN(LineHeightType, "lineHeightType"),
    PN(TabPositions, "tabPositions"),
    PN(HeadingLevel, "headingLevel"),
    PN(BlockQuoteLevel, "blockQuoteLevel"),
    PN(BlockCodeLanguage, "blockCodeLanguage"),
    PN(BlockCodeFence, "blockCodeFence"),
    PN(BlockMarker, "blockMarker"),
    PN(BlockTrailingHorizontalRulerWidth, "blockTrailingHorizontalRulerWidth"),
    PN(PageBreakPolicy, "pageBreakPolicy"),
    // --- frame format ---
    PN(FrameBorder, "frameBorder"),
    PN(FrameBorderBrush, "frameBorderBrush"),
    PN(FrameBorderStyle, "frameBorderStyle"),
    PN(FrameMargin, "frameMargin"),
    PN(FramePadding, "framePadding"),
    PN(FrameWidth, "frameWidth"),
    PN(FrameHeight, "frameHeight"),
    PN(FrameTopMargin, "frameTopMargin"),
    PN(FrameBottomMargin, "frameBottomMargin"),
    PN(FrameLeftMargin, "frameLeftMargin"),
    PN(FrameRightMargin, "frameRightMargin"),
    // --- table format ---
    PN(TableColumns, "tableColumns"),
    PN(TableColumnWidthConstraints, "columnWidths"),
    PN(TableCellSpacing, "cellSpacing"),
    PN(TableCellPadding, "cellPadding"),
    PN(TableHeaderRowCount, "headerRowCount"),
    PN(TableBorderCollapse, "tableBorderCollapse"),
    PN(TableCellRowSpan, "tableCellRowSpan"),
    PN(TableCellColumnSpan, "tableCellColumnSpan"),
    PN(TableCellTopPadding, "tableCellTopPadding"),
    PN(TableCellBottomPadding, "tableCellBottomPadding"),
    PN(TableCellLeftPadding, "tableCellLeftPadding"),
    PN(TableCellRightPadding, "tableCellRightPadding"),
    PN(TableCellTopBorder, "tableCellTopBorder"),
    PN(TableCellBottomBorder, "tableCellBottomBorder"),
    PN(TableCellLeftBorder, "tableCellLeftBorder"),
    PN(TableCellRightBorder, "tableCellRightBorder"),
    PN(TableCellTopBorderStyle, "tableCellTopBorderStyle"),
    PN(TableCellBottomBorderStyle, "tableCellBottomBorderStyle"),
    PN(TableCellLeftBorderStyle, "tableCellLeftBorderStyle"),
    PN(TableCellRightBorderStyle, "tableCellRightBorderStyle"),
    PN(TableCellTopBorderBrush, "tableCellTopBorderBrush"),
    PN(TableCellBottomBorderBrush, "tableCellBottomBorderBrush"),
    PN(TableCellLeftBorderBrush, "tableCellLeftBorderBrush"),
    PN(TableCellRightBorderBrush, "tableCellRightBorderBrush"),
    // --- list format ---
    PN(ListStyle, "listStyle"),
    PN(ListIndent, "listIndent"),
    PN(ListNumberPrefix, "numberPrefix"),
    PN(ListNumberSuffix, "numberSuffix"),
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    PN(ListStart, "listStart"),
#endif
};

#undef PN

const QHash<int, QString> &propIdToNameMap()
{
    static const QHash<int, QString> h = [] {
        QHash<int, QString> m;
        for (const PropName &p : kPropNames)
            m.insert(p.id, QString::fromLatin1(p.name));
        return m;
    }();
    return h;
}

const QHash<QString, int> &propNameToIdMap()
{
    static const QHash<QString, int> h = [] {
        QHash<QString, int> m;
        for (const PropName &p : kPropNames)
            m.insert(QString::fromLatin1(p.name), p.id);
        return m;
    }();
    return h;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
// QFont::Tag packs a four-character OpenType tag (e.g. "liga") into a quint32,
// big-endian as in the OpenType spec. Render printable tags as text for
// readability, anything else as hex; both forms parse back exactly, so the
// choice is cosmetic only. (If QFont::Tag ever stops being quint32-based, the
// QMap branches below simply won't match and values fall back to "unknown"
// with a warning — no compile break, no crash.)
static QString fontTagToString(quint32 tag)
{
    const char bytes[4] = {
        char(tag >> 24), char((tag >> 16) & 0xFF), char((tag >> 8) & 0xFF), char(tag & 0xFF)
    };
    bool printable = true;
    for (char c : bytes) {
        if (c < 0x20 || c > 0x7E)
            printable = false;
    }
    if (printable)
        return QString::fromLatin1(bytes, 4);
    return QStringLiteral("0x") + QString::number(tag, 16).toUpper();
}

static quint32 fontTagFromString(const QString &s, bool *ok)
{
    if (s.startsWith(QLatin1String("0x")))
        return s.mid(2).toUInt(ok, 16);
    const QByteArray b = s.toLatin1();
    if (b.size() == 4) {
        if (ok)
            *ok = true;
        return (quint32(quint8(b[0])) << 24) | (quint32(quint8(b[1])) << 16)
             | (quint32(quint8(b[2])) << 8) | quint32(quint8(b[3]));
    }
    if (ok)
        *ok = false;
    return 0;
}
#endif

QString TextDocumentJson::propertyName(int id)
{
    auto it = propIdToNameMap().constFind(id);
    if (it != propIdToNameMap().constEnd())
        return it.value();
    return QStringLiteral("prop_%1").arg(id);
}

int TextDocumentJson::propertyId(const QString &name)
{
    auto it = propNameToIdMap().constFind(name);
    if (it != propNameToIdMap().constEnd())
        return it.value();
    if (name.startsWith(QLatin1String("prop_"))) {
        bool ok = false;
        const int id = name.mid(5).toInt(&ok);
        if (ok)
            return id;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// QVariant <-> tagged JSON. Every value carries a short type tag so the
// reader can reconstruct the exact QVariant type.
// ---------------------------------------------------------------------------

QJsonObject TextDocumentJson::variantToJson(const QVariant &value)
{
    QJsonObject o;
    const QMetaType mt = value.metaType();

    switch (mt.id()) {
    case QMetaType::Bool:       o["t"] = "b";   o["v"] = value.toBool(); break;
    case QMetaType::Int:        o["t"] = "i";   o["v"] = value.toInt(); break;
    case QMetaType::UInt:       o["t"] = "u";   o["v"] = qint64(value.toUInt()); break;
    case QMetaType::LongLong:   o["t"] = "i64"; o["v"] = value.toLongLong(); break;
    case QMetaType::ULongLong:  o["t"] = "u64"; o["v"] = QString::number(value.toULongLong()); break;
    case QMetaType::Float:
    case QMetaType::Double:     o["t"] = "d";   o["v"] = value.toDouble(); break;
    case QMetaType::QString:    o["t"] = "s";   o["v"] = value.toString(); break;
    case QMetaType::QStringList:
        o["t"] = "sl";
        o["v"] = QJsonArray::fromStringList(value.toStringList());
        break;
    default: break;
    }
    if (o.contains("t"))
        return o;

    if (mt == QMetaType::fromType<QColor>()) {
        o["t"] = "color";
        o["v"] = value.value<QColor>().name(QColor::HexArgb);
    } else if (mt == QMetaType::fromType<QFont>()) {
        o["t"] = "font";
        o["v"] = value.value<QFont>().toString();
    } else if (mt == QMetaType::fromType<QBrush>()) {
        const QBrush b = value.value<QBrush>();
        QJsonObject bv;
        bv["style"] = int(b.style());
        bv["color"] = b.color().name(QColor::HexArgb);
        o["t"] = "brush";
        o["v"] = bv;
    } else if (mt == QMetaType::fromType<QPen>()) {
        const QPen p = value.value<QPen>();
        QJsonObject pv;
        pv["style"] = int(p.style());
        pv["width"] = p.widthF();
        pv["color"] = p.color().name(QColor::HexArgb);
        o["t"] = "pen";
        o["v"] = pv;
    } else if (mt == QMetaType::fromType<QTextLength>()) {
        const QTextLength l = value.value<QTextLength>();
        QJsonObject lv;
        lv["m"] = int(l.type());
        lv["v"] = l.rawValue();
        o["t"] = "tl";
        o["v"] = lv;
    } else if (mt == QMetaType::fromType<QVector<QTextLength>>()) {
        QJsonArray arr;
        for (const QTextLength &l : value.value<QVector<QTextLength>>()) {
            QJsonObject lv;
            lv["m"] = int(l.type());
            lv["v"] = l.rawValue();
            arr.append(lv);
        }
        o["t"] = "tlv";
        o["v"] = arr;
    } else if (mt == QMetaType::fromType<QSizeF>()) {
        const QSizeF s = value.value<QSizeF>();
        QJsonObject sv;
        sv["w"] = s.width();
        sv["h"] = s.height();
        o["t"] = "sizef";
        o["v"] = sv;
    } else if (mt == QMetaType::fromType<QUrl>()) {
        o["t"] = "url";
        o["v"] = value.value<QUrl>().toString();
    } else if (mt == QMetaType::fromType<QList<QTextOption::Tab>>()) {
        // Kept for safety; per the Qt docs TabPositions is actually stored as
        // a QList<QVariant> of single tabs (handled below), not as
        // QList<QTextOption::Tab>.
        QJsonArray arr;
        for (const QTextOption::Tab &tab : value.value<QList<QTextOption::Tab>>()) {
            QJsonObject t;
            t["p"] = tab.position;
            t["t"] = int(tab.type);
            t["d"] = QString(tab.delimiter);
            arr.append(t);
        }
        o["t"] = "tabs";
        o["v"] = arr;
    } else if (mt == QMetaType::fromType<QTextOption::Tab>()) {
        // Elements of the TabPositions list (QList<QVariant>).
        const QTextOption::Tab tab = value.value<QTextOption::Tab>();
        QJsonObject tv;
        tv["p"] = tab.position;
        tv["t"] = int(tab.type);
        tv["d"] = QString(tab.delimiter);
        o["t"] = "tab";
        o["v"] = tv;
    } else if (mt.id() == QMetaType::QVariantList) {
        // Generic: covers TabPositions and any future list-typed property.
        QJsonArray arr;
        for (const QVariant &e : value.toList())
            arr.append(variantToJson(e));
        o["t"] = "vlist";
        o["v"] = arr;
    } else if (mt.id() == QMetaType::QVariantMap) {
        QJsonObject m;
        const QVariantMap vm = value.toMap();
        for (auto it = vm.constBegin(); it != vm.constEnd(); ++it)
            m.insert(it.key(), variantToJson(it.value()));
        o["t"] = "vmap";
        o["v"] = m;
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
    } else if (mt == QMetaType::fromType<QMap<quint32, quint32>>()) {
        // QTextFormat::FontFeatures (Qt 6.11+): QMap<QFont::Tag, quint32>.
        QJsonObject m;
        const auto fm = value.value<QMap<quint32, quint32>>();
        for (auto it = fm.constBegin(); it != fm.constEnd(); ++it)
            m.insert(fontTagToString(it.key()), QJsonValue(qint64(it.value())));
        o["t"] = "fontFeatures";
        o["v"] = m;
    } else if (mt == QMetaType::fromType<QMap<quint32, float>>()) {
        // QTextFormat::FontVariableAxes (Qt 6.11+): QMap<QFont::Tag, float>.
        QJsonObject m;
        const auto am = value.value<QMap<quint32, float>>();
        for (auto it = am.constBegin(); it != am.constEnd(); ++it)
            m.insert(fontTagToString(it.key()), QJsonValue(double(it.value())));
        o["t"] = "fontVariableAxes";
        o["v"] = m;
#endif
    } else {
        // Shouldn't happen for real format properties, but never drop data.
        qWarning() << "TextDocumentJson: no JSON mapping for property type"
                   << mt.name() << "- storing debug string only";
        o["t"] = "unknown";
        o["v"] = value.toString();
        o["type"] = QString::fromLatin1(mt.name());
    }
    return o;
}

QVariant TextDocumentJson::jsonToVariant(const QJsonObject &obj)
{
    const QString t = obj.value("t").toString();
    const QJsonValue v = obj.value("v");

    if (t == "b")   return v.toBool();
    if (t == "i")   return v.toInt();
    if (t == "u")   return QVariant::fromValue(static_cast<uint>(v.toDouble()));
    if (t == "i64") return v.toVariant().toLongLong();
    if (t == "u64") return v.toString().toULongLong();
    if (t == "d")   return v.toDouble();
    if (t == "s")   return v.toString();
    if (t == "sl") {
        QStringList sl;
        for (const QJsonValue &sv : v.toArray())
            sl << sv.toString();
        return sl;
    }
    if (t == "color") return QColor(v.toString());
    if (t == "font") {
        QFont f;
        f.fromString(v.toString());
        return f;
    }
    if (t == "brush") {
        const QJsonObject bv = v.toObject();
        const auto style = Qt::BrushStyle(bv.value("style").toInt());
        const QColor col(bv.value("color").toString());
        if (style == Qt::SolidPattern) return QBrush(col);
        if (style == Qt::NoBrush)      return QBrush(Qt::NoBrush);
        return QBrush(col, style);
    }
    if (t == "pen") {
        const QJsonObject pv = v.toObject();
        QPen p(QColor(pv.value("color").toString()));
        p.setWidthF(pv.value("width").toDouble());
        p.setStyle(Qt::PenStyle(pv.value("style").toInt()));
        return p;
    }
    if (t == "tl") {
        const QJsonObject lv = v.toObject();
        return QVariant::fromValue(QTextLength(QTextLength::Type(lv.value("m").toInt()),
                                              lv.value("v").toDouble()));
    }
    if (t == "tlv") {
        QVector<QTextLength> vec;
        for (const QJsonValue &lvv : v.toArray()) {
            const QJsonObject lv = lvv.toObject();
            vec.append(QTextLength(QTextLength::Type(lv.value("m").toInt()),
                                   lv.value("v").toDouble()));
        }
        return QVariant::fromValue(vec);
    }
    if (t == "sizef") {
        const QJsonObject sv = v.toObject();
        return QSizeF(sv.value("w").toDouble(), sv.value("h").toDouble());
    }
    if (t == "url") return QUrl(v.toString());
    if (t == "tabs") {
        QList<QTextOption::Tab> tabs;
        for (const QJsonValue &tv : v.toArray()) {
            const QJsonObject to = tv.toObject();
            const QString d = to.value("d").toString();
            tabs.append(QTextOption::Tab(to.value("p").toDouble(),
                                         QTextOption::TabType(to.value("t").toInt()),
                                         d.isEmpty() ? QChar() : d.at(0)));
        }
        return QVariant::fromValue(tabs);
    }
    if (t == "tab") {
        const QJsonObject tv = v.toObject();
        const QString d = tv.value("d").toString();
        return QVariant::fromValue(QTextOption::Tab(
            tv.value("p").toDouble(),
            QTextOption::TabType(tv.value("t").toInt()),
            d.isEmpty() ? QChar() : d.at(0)));
    }
    if (t == "vlist") {
        QVariantList l;
        const QJsonArray arr = v.toArray();
        l.reserve(arr.size());
        for (const QJsonValue &e : arr)
            l.append(jsonToVariant(e));
        return l;
    }
    if (t == "vmap") {
        QVariantMap m;
        const QJsonObject mo = v.toObject();
        for (auto it = mo.constBegin(); it != mo.constEnd(); ++it)
            m.insert(it.key(), jsonToVariant(it.value()));
        return m;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
    if (t == "fontFeatures") {
        QMap<quint32, quint32> m;
        const QJsonObject mo = v.toObject();
        for (auto it = mo.constBegin(); it != mo.constEnd(); ++it) {
            bool ok = false;
            const quint32 tag = fontTagFromString(it.key(), &ok);
            if (ok)
                m.insert(tag, quint32(it.value().toDouble()));
        }
        return QVariant::fromValue(m);
    }
    if (t == "fontVariableAxes") {
        QMap<quint32, float> m;
        const QJsonObject mo = v.toObject();
        for (auto it = mo.constBegin(); it != mo.constEnd(); ++it) {
            bool ok = false;
            const quint32 tag = fontTagFromString(it.key(), &ok);
            if (ok)
                m.insert(tag, float(it.value().toDouble()));
        }
        return QVariant::fromValue(m);
    }
#endif
    qWarning() << "TextDocumentJson: cannot restore value of type"
               << obj.value("type").toString() << "- skipping";
    return QVariant();
}

// QJsonValue overload: unwraps tagged {"t","v"} objects via the QJsonObject
// overload above; converts a bare (untagged) value best-effort. Bare values
// only occur in hand-edited JSON, since variantToJson always emits tagged
// objects, so this path is a graceful fallback, not the normal route.
QVariant TextDocumentJson::jsonToVariant(const QJsonValue &val)
{
    if (val.isObject())
        return jsonToVariant(val.toObject());
    switch (val.type()) {
    case QJsonValue::Bool:   return val.toBool();
    case QJsonValue::String: return val.toString();
    case QJsonValue::Double: return val.toDouble();
    case QJsonValue::Array: {
        QVariantList l;
        const QJsonArray arr = val.toArray();
        l.reserve(arr.size());
        for (const QJsonValue &e : arr)
            l.append(jsonToVariant(e));
        return l;
    }
    case QJsonValue::Null:
    case QJsonValue::Undefined:
    default:
        return QVariant();
    }
}

QJsonObject TextDocumentJson::formatToJson(const QTextFormat &format)
{
    QJsonObject props;
    const QMap<int, QVariant> pm = format.properties();
    for (auto it = pm.constBegin(); it != pm.constEnd(); ++it)
        props.insert(propertyName(it.key()), variantToJson(it.value()));
    QJsonObject o;
    o["properties"] = props;
    return o;
}

TextDocumentJson::ResourceWriter TextDocumentJson::defaultResourceWriter()
{
    return [](const QString &, const QString &mimeType, const QByteArray &data) {
        QJsonObject o;
        o["mime"] = mimeType;
        o["data"] = QString::fromLatin1(data.toBase64());
        return o;
    };
}

TextDocumentJson::ResourceReader TextDocumentJson::defaultResourceReader()
{
    return [](const QString &, const QJsonObject &stored) {
        return QByteArray::fromBase64(stored.value("data").toString().toLatin1());
    };
}

// ---------------------------------------------------------------------------
// Serialize: walk the document model.
// ---------------------------------------------------------------------------

struct DocWalker {
    const QTextDocument *doc;
    TextDocumentJson::ResourceWriter writer;
    QJsonObject resources;
    QSet<QString> seenResources;
    int autoId = 0;

    // Returns the id under which the resource is stored.
    QString ensureResource(const QString &name, const QVariant &res)
    {
        QString id = name;
        if (id.isEmpty())
            id = QStringLiteral("img_%1").arg(++autoId);
        if (seenResources.contains(id))
            return id;
        seenResources.insert(id);

        QImage img;
        if (res.metaType() == QMetaType::fromType<QImage>())
            img = res.value<QImage>();
        else if (res.metaType() == QMetaType::fromType<QPixmap>())
            img = res.value<QPixmap>().toImage();

        if (img.isNull()) {
            qWarning() << "TextDocumentJson: image resource" << id << "has no image data";
            return id;
        }
        // QTextDocument doesn't retain the original file format, so
        // normalize to PNG (lossless).
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        resources.insert(id, writer(id, QStringLiteral("image/png"), png));
        return id;
    }

    QJsonObject blockToJson(const QTextBlock &block)
    {
        QJsonObject o;
        o["kind"] = "block";
        o["format"] = TextDocumentJson::formatToJson(block.blockFormat());
        QJsonArray segs;
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid())
                continue;
            const QTextCharFormat cf = frag.charFormat();
            if (cf.objectType() == QTextFormat::ImageObject) {
                QTextImageFormat imf = cf.toImageFormat();
                const QString id = ensureResource(
                    imf.name(),
                    doc->resource(QTextDocument::ImageResource, QUrl(imf.name())));
                imf.setName(id); // make sure the stored name matches the resource
                QJsonObject s;
                s["image"] = id;
                s["format"] = TextDocumentJson::formatToJson(imf);
                segs.append(s);
            } else {
                QJsonObject s;
                s["text"] = frag.text();
                s["format"] = TextDocumentJson::formatToJson(cf);
                segs.append(s);
            }
        }
        o["segments"] = segs;
        return o;
    }

    QJsonObject tableToJson(QTextTable *table)
    {
        QJsonObject o;
        o["kind"] = "table";
        o["format"] = TextDocumentJson::formatToJson(table->format());
        o["rows"] = table->rows();
        o["columns"] = table->columns();
        QJsonArray cells;
        for (int r = 0; r < table->rows(); ++r) {
            for (int c = 0; c < table->columns(); ++c) {
                const QTextTableCell cell = table->cellAt(r, c);
                if (!cell.isValid())
                    continue;
                if (cell.row() != r || cell.column() != c)
                    continue; // covered by a merged span
                QJsonObject co;
                co["row"] = r;
                co["col"] = c;
                co["rowSpan"] = cell.rowSpan();
                co["colSpan"] = cell.columnSpan();
                // Each cell's content lives in its own inner frame.
                QTextCursor cc = cell.firstCursorPosition();
                QTextFrame *cellFrame = cc.currentFrame();
                if (cellFrame && cellFrame != table)
                    co["content"] = childrenToJson(cellFrame);
                else
                    co["content"] = QJsonArray();
                cells.append(co);
            }
        }
        o["cells"] = cells;
        return o;
    }

    QJsonArray childrenToJson(QTextFrame *frame)
    {
        QJsonArray children;
        // Consecutive blocks sharing a QTextList are grouped into one list node.
        QTextList *openList = nullptr;
        QJsonArray listItems;
        QJsonObject listFormat;

        auto flushList = [&]() {
            if (openList) {
                QJsonObject lo;
                lo["kind"] = "list";
                lo["format"] = listFormat;
                lo["items"] = listItems;
                children.append(lo);
                openList = nullptr;
                listItems = QJsonArray();
            }
        };

        for (QTextFrame::iterator it = frame->begin(); !it.atEnd(); ++it) {
            if (QTextTable *t = qobject_cast<QTextTable *>(it.currentFrame())) {
                flushList();
                children.append(tableToJson(t));
            } else if (QTextFrame *f = it.currentFrame()) {
                flushList();
                QJsonObject fo;
                fo["kind"] = "frame";
                fo["format"] = TextDocumentJson::formatToJson(f->frameFormat());
                fo["children"] = childrenToJson(f);
                children.append(fo);
            } else {
                const QTextBlock b = it.currentBlock();
                if (!b.isValid())
                    continue;
                if (QTextList *l = b.textList()) {
                    if (l != openList) {
                        flushList();
                        openList = l;
                        listFormat = TextDocumentJson::formatToJson(l->format());
                    }
                    listItems.append(blockToJson(b));
                } else {
                    flushList();
                    children.append(blockToJson(b));
                }
            }
        }
        flushList();
        return children;
    }
};

QJsonObject TextDocumentJson::serialize(const QTextDocument *doc, ResourceWriter writer)
{
    QJsonObject root;
    root["format"] = QStringLiteral("qtextdocument");
    root["version"] = 1;

    const QSizeF ps = doc->pageSize();
    QJsonObject psz;
    psz["w"] = ps.width();
    psz["h"] = ps.height();
    root["pageSize"] = psz;
    root["documentMargin"] = doc->documentMargin();
    root["defaultFont"] = doc->defaultFont().toString();

    DocWalker w{ doc, writer, QJsonObject(), QSet<QString>(), 0 };
    QJsonObject content;
    content["kind"] = QStringLiteral("frame");
    content["format"] = formatToJson(doc->rootFrame()->frameFormat());
    content["children"] = w.childrenToJson(doc->rootFrame());
    root["content"] = content;
    root["resources"] = w.resources;
    return root;
}

// ---------------------------------------------------------------------------
// Deserialize: rebuild the document with QTextCursor.
// Strategy: everything is appended in order; before inserting each child we
// re-anchor the cursor to the container's end, so we never depend on where
// Qt left the cursor after the previous insertion.
// ---------------------------------------------------------------------------

struct DocBuilder {
    QTextDocument *doc = nullptr;

    void appendSegments(QTextCursor &cursor, const QJsonArray &segs)
    {
        for (const QJsonValue &sv : segs) {
            const QJsonObject s = sv.toObject();
            if (s.contains("image")) {
                QTextImageFormat imf =
                    TextDocumentJson::jsonToFormat<QTextImageFormat>(s.value("format").toObject());
                if (imf.name().isEmpty())
                    imf.setName(s.value("image").toString());
                cursor.insertImage(imf); // image bytes come from doc resources
            } else {
                const QTextCharFormat cf =
                    TextDocumentJson::jsonToFormat<QTextCharFormat>(s.value("format").toObject());
                cursor.insertText(s.value("text").toString(), cf);
            }
        }
    }

    void appendBlockContent(QTextCursor &cursor, const QJsonObject &blockObj)
    {
        cursor.setBlockFormat(
            TextDocumentJson::jsonToFormat<QTextBlockFormat>(blockObj.value("format").toObject()));
        appendSegments(cursor, blockObj.value("segments").toArray());
    }

    void appendList(QTextFrame *container, const QJsonObject &o)
    {
        const QJsonArray items = o.value("items").toArray();
        if (items.isEmpty())
            return;
        QTextCursor cursor(doc);
        cursor.setPosition(container->lastPosition());

        cursor.insertList(
            TextDocumentJson::jsonToFormat<QTextListFormat>(o.value("format").toObject()));
        // insertList() creates the first item block for us.
        appendBlockContent(cursor, items.at(0).toObject());
        for (int i = 1; i < items.size(); ++i) {
            cursor.insertBlock(); // continues the list
            appendBlockContent(cursor, items.at(i).toObject());
        }
        // Terminate the list with a detached trailing block (what Qt editors do).
        cursor.insertBlock(QTextBlockFormat());
        if (QTextList *l = cursor.block().textList())
            l->remove(cursor.block());
    }

    void appendTable(QTextFrame *container, const QJsonObject &o)
    {
        const int rows = o.value("rows").toInt();
        const int cols = o.value("columns").toInt();
        if (rows <= 0 || cols <= 0)
            return;
        QTextCursor cursor(doc);
        cursor.setPosition(container->lastPosition());

        QTextTable *table = cursor.insertTable(
            rows, cols,
            TextDocumentJson::jsonToFormat<QTextTableFormat>(o.value("format").toObject()));

        const QJsonArray cells = o.value("cells").toArray();
        // Merges first, so cellAt() below sees the final grid.
        for (const QJsonValue &cv : cells) {
            const QJsonObject co = cv.toObject();
            const int rs = co.value("rowSpan").toInt(1);
            const int cs = co.value("colSpan").toInt(1);
            if (rs > 1 || cs > 1)
                table->mergeCells(co.value("row").toInt(), co.value("col").toInt(), rs, cs);
        }
        // Then content.
        for (const QJsonValue &cv : cells) {
            const QJsonObject co = cv.toObject();
            const QTextTableCell cell =
                table->cellAt(co.value("row").toInt(), co.value("col").toInt());
            if (!cell.isValid())
                continue;
            QTextCursor cc = cell.firstCursorPosition();
            QTextFrame *cellFrame = cc.currentFrame();
            if (cellFrame && cellFrame != table)
                appendChildren(cellFrame, co.value("content").toArray(), true);
        }
    }

    void appendFrame(QTextFrame *container, const QJsonObject &o)
    {
        QTextCursor cursor(doc);
        cursor.setPosition(container->lastPosition());
        QTextFrame *f = cursor.insertFrame(
            TextDocumentJson::jsonToFormat<QTextFrameFormat>(o.value("format").toObject()));
        appendChildren(f, o.value("children").toArray(), true);
    }

    void appendChildren(QTextFrame *container, const QJsonArray &children, bool fresh)
    {
        QTextCursor cursor(doc);
        bool first = fresh; // reuse the container's initial empty block for a leading block
        for (const QJsonValue &cv : children) {
            cursor.setPosition(container->lastPosition());
            const QJsonObject o = cv.toObject();
            const QString kind = o.value("kind").toString();
            if (kind == "block") {
                const QTextBlockFormat bf =
                    TextDocumentJson::jsonToFormat<QTextBlockFormat>(o.value("format").toObject());
                if (first)
                    cursor.setBlockFormat(bf);
                else
                    cursor.insertBlock(bf);
                appendSegments(cursor, o.value("segments").toArray());
            } else if (kind == "list") {
                appendList(container, o);
            } else if (kind == "table") {
                appendTable(container, o);
            } else if (kind == "frame") {
                appendFrame(container, o);
            } else {
                qWarning() << "TextDocumentJson: unknown node kind" << kind << "- skipping";
            }
            first = false;
        }
    }
};

QTextDocument *TextDocumentJson::deserialize(const QJsonObject &obj, ResourceReader reader)
{
    QTextDocument *doc = new QTextDocument;

    // Resources first: images must exist before fragments reference them.
    const QJsonObject resObj = obj.value("resources").toObject();
    for (auto it = resObj.constBegin(); it != resObj.constEnd(); ++it) {
        const QByteArray data = reader(it.key(), it.value().toObject());
        QImage img;
        if (!data.isEmpty())
            img.loadFromData(data);
        if (!img.isNull())
            doc->addResource(QTextDocument::ImageResource, QUrl(it.key()), img);
        else
            qWarning() << "TextDocumentJson: could not load image resource" << it.key();
    }

    if (obj.contains("pageSize")) {
        const QJsonObject ps = obj.value("pageSize").toObject();
        doc->setPageSize(QSizeF(ps.value("w").toDouble(), ps.value("h").toDouble()));
    }
    if (obj.contains("documentMargin"))
        doc->setDocumentMargin(obj.value("documentMargin").toDouble());
    if (obj.contains("defaultFont")) {
        QFont f;
        f.fromString(obj.value("defaultFont").toString());
        doc->setDefaultFont(f);
    }

    const QJsonObject content = obj.value("content").toObject();
    if (content.value("kind").toString() != QLatin1String("frame")) {
        qWarning() << "TextDocumentJson: content root is not a frame";
        return doc;
    }
    doc->rootFrame()->setFrameFormat(
        jsonToFormat<QTextFrameFormat>(content.value("format").toObject()));

    DocBuilder b;
    b.doc = doc;
    b.appendChildren(doc->rootFrame(), content.value("children").toArray(), true);
    return doc;
}
