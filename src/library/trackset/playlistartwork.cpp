// AI-generated (Claude Code). Start of AI-generated text.
#include "library/trackset/playlistartwork.h"

#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <Qt>

namespace mixxx {
namespace library {

QByteArray makePlaylistArtworkFromFile(const QString& filePath) {
    // Read-only: the picture of the user is never changed.
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    QImageReader reader(&file);
    reader.setAutoTransform(true);
    const QByteArray format = reader.format();
    if (format != "jpeg" && format != "png") {
        return QByteArray();
    }
    QImage image = reader.read();
    if (image.isNull()) {
        return QByteArray();
    }
    if (image.width() > kMaxPlaylistArtworkEdge ||
            image.height() > kMaxPlaylistArtworkEdge) {
        image = image.scaled(kMaxPlaylistArtworkEdge,
                kMaxPlaylistArtworkEdge,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
    }
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        return QByteArray();
    }
    return png;
}

QImage playlistArtworkImage(const QByteArray& storedBytes) {
    if (storedBytes.isEmpty()) {
        return QImage();
    }
    QImage image;
    if (!image.loadFromData(storedBytes, "PNG")) {
        return QImage();
    }
    return image;
}

} // namespace library
} // namespace mixxx

// End of AI-generated text.
