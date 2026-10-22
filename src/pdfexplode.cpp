#include "pdfexplode.h"

#include <QDir>
#include <QFileInfo>

#ifdef QTNOTEBOOK_HAS_QTPDF
#include <QPdfDocument>
#include <QImage>
#include <QPainter>
#endif

bool PdfExplode::isAvailable() {
#ifdef QTNOTEBOOK_HAS_QTPDF
    return true;
#else
    return false;
#endif
}

PdfExplode::Result PdfExplode::explodePdf(const QString& pdfPath, const QString& outputDir) {
    Result result;
#ifdef QTNOTEBOOK_HAS_QTPDF
    QPdfDocument document;
    const QPdfDocument::Error loadError = document.load(pdfPath);
    if (loadError != QPdfDocument::Error::None) {
        result.error = QStringLiteral("Could not open PDF %1 (QtPdf error %2)")
                           .arg(pdfPath).arg(static_cast<int>(loadError));
        return result;
    }
    result.pageCount = document.pageCount();
    if (result.pageCount <= 0) {
        result.error = QStringLiteral("PDF %1 has no pages").arg(pdfPath);
        return result;
    }
    if (result.pageCount > kLargePdfWarningPages) {
        result.warning = QStringLiteral("Large PDF: %1 pages (warning threshold is %2). "
                                        "Exploding is meant for small documents like character sheets.")
                             .arg(result.pageCount).arg(kLargePdfWarningPages);
    }
    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        result.error = QStringLiteral("Could not create output folder %1").arg(outputDir);
        return result;
    }
    const qreal scale = kRenderDpi / 72.0;   // PDF points are 1/72 inch
    for (int page = 0; page < result.pageCount; ++page) {
        const QSizeF pointSize = document.pagePointSize(page);
        const QSize imageSize(qRound(pointSize.width() * scale), qRound(pointSize.height() * scale));
        const QImage rendered = document.render(page, imageSize);
        if (rendered.isNull()) {
            result.error = QStringLiteral("Rendering page %1 of %2 failed").arg(page + 1).arg(pdfPath);
            result.pngPaths.clear();
            return result;
        }
        // QPdfDocument renders onto transparency (alpha 0 where the PDF
        // paints nothing). Exploded pages are paper: flatten onto white
        // so a page occludes what's behind it on the canvas and prints
        // as it looked in the PDF.
        QImage image(rendered.size(), QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.drawImage(0, 0, rendered);
        painter.end();
        const QString pngPath = dir.filePath(QStringLiteral("page-%1.png").arg(page + 1));
        if (!image.save(pngPath, "PNG")) {
            result.error = QStringLiteral("Could not save rendered page to %1").arg(pngPath);
            result.pngPaths.clear();
            return result;
        }
        result.pngPaths.append(pngPath);
    }
    result.ok = true;
    return result;
#else
    Q_UNUSED(outputDir);
    if (!QFileInfo::exists(pdfPath))
        result.error = QStringLiteral("PDF file not found: %1").arg(pdfPath);
    else
        result.error = QStringLiteral("QtPdf not available in this build — PDF import is disabled. "
                                      "Build against a Qt that ships the Pdf module to enable exploding PDFs.");
    return result;
#endif
}
