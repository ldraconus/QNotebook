#include "zipstore.h"

#include <QFile>

// Little-endian field helpers (file-scope, named via the ZipStore class API).

static void put16(QByteArray& out, quint16 value) {
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

static void put32(QByteArray& out, quint32 value) {
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

static quint16 get16(const QByteArray& data, int offset) {
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset)))
        | static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1)) << 8);
}

static quint32 get32(const QByteArray& data, int offset) {
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset)))
        | (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8)
        | (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16)
        | (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24);
}

quint32 ZipStore::crc32(const QByteArray& data) {
    static quint32 table[256];
    static bool tableReady = false;
    if (!tableReady) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        tableReady = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (char ch : data) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        crc = table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

bool ZipStore::writeFile(const QString& filePath, const QMap<QString, QByteArray>& entries, QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr)
            *error = QStringLiteral("Cannot open %1 for writing: %2").arg(filePath, file.errorString());
        return false;
    }

    struct CentralInfo {
        QByteArray nameUtf8;
        quint32 crc = 0;
        quint32 size = 0;
        quint32 offset = 0;
    };
    QList<CentralInfo> central;
    qint64 offset = 0;

    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const QByteArray nameUtf8 = it.key().toUtf8();
        const QByteArray& data = it.value();
        const quint32 crc = crc32(data);

        QByteArray header;
        put32(header, 0x04034b50u);              // local file header
        put16(header, 20);                       // version needed
        put16(header, 0x0800);                   // flag: UTF-8 names
        put16(header, 0);                        // method: stored
        put16(header, 0);                        // mod time
        put16(header, 0x21);                     // mod date (1980-01-01-ish, arbitrary valid)
        put32(header, crc);
        put32(header, static_cast<quint32>(data.size()));
        put32(header, static_cast<quint32>(data.size()));
        put16(header, static_cast<quint16>(nameUtf8.size()));
        put16(header, 0);                        // extra length
        header.append(nameUtf8);

        if (file.write(header) != header.size() || file.write(data) != data.size()) {
            if (error != nullptr)
                *error = QStringLiteral("Write failed for %1: %2").arg(filePath, file.errorString());
            return false;
        }
        central.append(CentralInfo{ nameUtf8, crc, static_cast<quint32>(data.size()), static_cast<quint32>(offset) });
        offset += header.size() + data.size();
    }

    const qint64 centralStart = offset;
    for (const CentralInfo& info : central) {
        QByteArray record;
        put32(record, 0x02014b50u);              // central directory record
        put16(record, 20);                       // version made by
        put16(record, 20);                       // version needed
        put16(record, 0x0800);                   // UTF-8
        put16(record, 0);                        // stored
        put16(record, 0);
        put16(record, 0x21);
        put32(record, info.crc);
        put32(record, info.size);
        put32(record, info.size);
        put16(record, static_cast<quint16>(info.nameUtf8.size()));
        put16(record, 0);                        // extra
        put16(record, 0);                        // comment
        put16(record, 0);                        // disk number
        put16(record, 0);                        // internal attrs
        put32(record, 0);                        // external attrs
        put32(record, info.offset);
        record.append(info.nameUtf8);
        if (file.write(record) != record.size()) {
            if (error != nullptr)
                *error = QStringLiteral("Write failed finishing %1: %2").arg(filePath, file.errorString());
            return false;
        }
        offset += record.size();
    }

    QByteArray end;
    put32(end, 0x06054b50u);                     // end of central directory
    put16(end, 0);
    put16(end, 0);
    put16(end, static_cast<quint16>(central.size()));
    put16(end, static_cast<quint16>(central.size()));
    put32(end, static_cast<quint32>(offset - centralStart));
    put32(end, static_cast<quint32>(centralStart));
    put16(end, 0);
    if (file.write(end) != end.size() || !file.flush()) {
        if (error != nullptr)
            *error = QStringLiteral("Write failed closing %1: %2").arg(filePath, file.errorString());
        return false;
    }
    file.close();
    return true;
}

bool ZipStore::readFile(const QString& filePath, QMap<QString, QByteArray>* entries, QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr)
            *error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }
    const QByteArray all = file.readAll();
    file.close();

    // Find the End Of Central Directory record (scan back over the comment range).
    int eocd = -1;
    const int minPos = qMax(0, all.size() - 22 - 65536);
    for (int i = all.size() - 22; i >= minPos; --i) {
        if (get32(all, i) == 0x06054b50u) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        if (error != nullptr)
            *error = QStringLiteral("%1 is not a readable zip file (no end-of-central-directory)").arg(filePath);
        return false;
    }
    const int count = get16(all, eocd + 10);
    quint32 pos = get32(all, eocd + 16);

    QMap<QString, QByteArray> result;
    for (int i = 0; i < count; ++i) {
        if (pos + 46 > static_cast<quint32>(all.size()) || get32(all, static_cast<int>(pos)) != 0x02014b50u) {
            if (error != nullptr)
                *error = QStringLiteral("%1 has a corrupt central directory").arg(filePath);
            return false;
        }
        const quint32 expectedCrc = get32(all, static_cast<int>(pos) + 16);
        const quint32 size = get32(all, static_cast<int>(pos) + 20);
        const int nameLen = get16(all, static_cast<int>(pos) + 28);
        const int extraLen = get16(all, static_cast<int>(pos) + 30);
        const int commentLen = get16(all, static_cast<int>(pos) + 32);
        const quint32 localOffset = get32(all, static_cast<int>(pos) + 42);
        const QString name = QString::fromUtf8(all.mid(static_cast<int>(pos) + 46, nameLen));

        if (localOffset + 30 > static_cast<quint32>(all.size()) || get32(all, static_cast<int>(localOffset)) != 0x04034b50u) {
            if (error != nullptr)
                *error = QStringLiteral("%1 has a corrupt local header for %2").arg(filePath, name);
            return false;
        }
        const int localNameLen = get16(all, static_cast<int>(localOffset) + 26);
        const int localExtraLen = get16(all, static_cast<int>(localOffset) + 28);
        const int dataStart = static_cast<int>(localOffset) + 30 + localNameLen + localExtraLen;
        const QByteArray data = all.mid(dataStart, static_cast<int>(size));
        if (data.size() != static_cast<int>(size) || crc32(data) != expectedCrc) {
            if (error != nullptr)
                *error = QStringLiteral("%1 entry %2 failed its CRC check").arg(filePath, name);
            return false;
        }
        result.insert(name, data);
        pos += 46 + nameLen + extraLen + commentLen;
    }
    if (entries != nullptr)
        *entries = result;
    return true;
}

bool ZipStore::listEntries(const QString& filePath, QStringList* names, QString* error) {
    QMap<QString, QByteArray> entries;
    if (!readFile(filePath, &entries, error))
        return false;
    if (names != nullptr)
        *names = entries.keys();
    return true;
}
