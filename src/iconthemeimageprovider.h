#pragma once
#include <QQuickImageProvider>

class IconThemeImageProvider : public QQuickImageProvider {
public:
    IconThemeImageProvider();
    QImage requestImage(const QString &id, QSize *realSize,
                        const QSize &requestedSize) override;
};
