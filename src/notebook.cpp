#include "notebook.h"

#include <QUuid>

QString Notebook::newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

Page* Notebook::findPage(const QString& pageId) {
    for (Section& section : sections) {
        for (Page& page : section.pages) {
            if (page.id == pageId)
                return &page;
        }
    }
    return nullptr;
}

const Page* Notebook::findPage(const QString& pageId) const {
    for (const Section& section : sections) {
        for (const Page& page : section.pages) {
            if (page.id == pageId)
                return &page;
        }
    }
    return nullptr;
}

Section* Notebook::findSection(const QString& sectionId) {
    for (Section& section : sections) {
        if (section.id == sectionId)
            return &section;
    }
    return nullptr;
}

int Notebook::pageCount() const {
    int total = 0;
    for (const Section& section : sections)
        total += section.pages.size();
    return total;
}
