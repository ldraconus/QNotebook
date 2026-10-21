cmake_minimum_required(VERSION 3.16)
project(QtNotebook VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
# QtPdf is optional: when present, PdfExplode renders for real; without it,
# the app still builds and PDF import reports that it is unavailable.
find_package(Qt6 QUIET COMPONENTS Pdf)

set(CORE_SOURCES
    src/textdocumentjson.cpp
    src/zipstore.cpp
    src/notebook.cpp
    src/notebookpackage.cpp
    src/pdfexplode.cpp
    src/elements/textelement.cpp
    src/elements/imageelement.cpp
)

add_library(notebookcore STATIC ${CORE_SOURCES})
target_include_directories(notebookcore PUBLIC src)
target_link_libraries(notebookcore PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
if(TARGET Qt6::Pdf)
    target_link_libraries(notebookcore PUBLIC Qt6::Pdf)
    target_compile_definitions(notebookcore PUBLIC QTNOTEBOOK_HAS_QTPDF=1)
    message(STATUS "QtPdf found: PDF explode will render for real")
else()
    message(STATUS "QtPdf NOT found: PDF explode will be a reporting stub")
endif()

add_executable(qtnotebook
    src/main.cpp
    src/mainwindow.cpp
    src/pagecanvas.cpp
)
target_link_libraries(qtnotebook PRIVATE notebookcore Qt6::PrintSupport)

add_executable(qtnotebook_smoke tests/smoke.cpp)
target_link_libraries(qtnotebook_smoke PRIVATE notebookcore)
