#pragma once

// pdfexplode.h — PDF import: consume a PDF and explode it into pages.
//
// Each page is rendered at 300 dpi to a PNG and placed on the canvas as an
// ordinary ImageElement, in order, so pages can be moved/resized/rearranged
// independently. Intended for SMALL PDFs (2-3 pages, e.g. HSCCU character
// sheets) — pageCount > kLargePdfWarningPages must be surfaced as a warning
// upstream before exploding.
//
// TODO(original-pdf): when real rendering lands, keep the source PDF in the
// package invisibly under the target page's .media/ folder as "_original.pdf"
// so a bad render never costs the user the original. The scaffold's caller
// (MainWindow::insertPdf) already notes where that hook goes.

#include <QString>
#include <QStringList>

class PdfExplode
{
public:
    static const int kLargePdfWarningPages = 10;
    static const int kRenderDpi = 300;

    struct Result
    {
        bool ok = false;
        QString error;          // set when ok == false
        QString warning;        // e.g. "large PDF" notice, may accompany ok == true
        int pageCount = 0;
        QStringList pngPaths;   // one PNG per page, in page order (files in outputDir)
    };

    // Render every page of pdfPath to outputDir/page-N.png at 300 dpi.
    // Without QtPdf this is a stub: ok == false and error explains why.
    static Result explodePdf(const QString& pdfPath, const QString& outputDir);

    static bool isAvailable();
};
