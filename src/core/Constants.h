#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

namespace ppocr {

inline constexpr const char* kProjectVersion = "1.0";
inline constexpr const char* kAppName = "PP-OCR Workbench";

inline QStringList imageExtensions() {
    return {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"};
}

inline QStringList pdfExtensions() {
    return {".pdf"};
}

inline QJsonObject defaultLabelSets() {
    return {
        {"doc_orientation", QJsonArray{"0", "90", "180", "270"}},
        {"textline_orientation", QJsonArray{"0", "180"}},
        {"layout", QJsonArray{"title", "text", "table", "image", "formula", "seal"}},
        {"table_classification", QJsonArray{"wired_table", "wireless_table"}},
        {"table_type", QJsonArray{"wired_table", "wireless_table"}},
    };
}

}  // namespace ppocr
