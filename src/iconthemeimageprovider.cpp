#include "iconthemeimageprovider.h"
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QDebug>

static QString s_themeName;
static QStringList s_themeSearchPaths;

static QString findIconFile(const QString &name, int size)
{
    static const QStringList exts = {"svg", "png", "xpm"};
    static const QStringList categories = {
        "apps", "actions", "places", "devices",
        "mimetypes", "mimes", "status", "emblems", "categories"
    };

    // Crule 结构: {base}/{theme}/{category}/{size}/{name}.ext
    //             {base}/{theme}/{category}/scalable/{name}.ext
    // hicolor 结构: {base}/{theme}/{size}x{size}/{category}/{name}.ext
    //               {base}/{theme}/scalable/{category}/{name}.ext

    const QStringList themes = {s_themeName, "hicolor"};

    for (const QString &theme : themes) {
        for (const QString &base : s_themeSearchPaths) {
            QString themeDir = base + "/" + theme;
            if (!QDir(themeDir).exists()) continue;

            for (const QString &cat : categories) {
                // 格式一：{category}/{size}/{name}  (Crule 风格)
                QStringList sizeDirs;
                sizeDirs << QString::number(size)
                         << "scalable"
                         << "22" << "24" << "32" << "48" << "64" << "128";

                for (const QString &sz : sizeDirs) {
                    for (const QString &ext : exts) {
                        QString path = themeDir + "/" + cat + "/" + sz
                                       + "/" + name + "." + ext;
                        if (QFileInfo::exists(path)) {
                            qDebug() << "[IconProvider] found:" << path;
                            return path;
                        }
                    }
                }

                // 格式二：{size}x{size}/{category}/{name}  (hicolor 风格)
                QStringList sizeXDirs;
                sizeXDirs << QString("%1x%1").arg(size)
                          << "scalable"
                          << "22x22" << "24x24" << "32x32"
                          << "48x48" << "64x64" << "128x128";

                for (const QString &sz : sizeXDirs) {
                    for (const QString &ext : exts) {
                        QString path = themeDir + "/" + sz + "/" + cat
                                       + "/" + name + "." + ext;
                        if (QFileInfo::exists(path)) {
                            qDebug() << "[IconProvider] found:" << path;
                            return path;
                        }
                    }
                }
            }
        }
    }

    if (name != QLatin1String("application-x-desktop"))
        return findIconFile("application-x-desktop", size);

    qDebug() << "[IconProvider] NOT FOUND:" << name;
    return QString();
}

IconThemeImageProvider::IconThemeImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
    QString cfgPath = QDir::homePath() + "/.config/cutefishos/theme.conf";
    QSettings cfg(cfgPath, QSettings::IniFormat);
    s_themeName = cfg.value("IconTheme", "hicolor").toString();
    s_themeSearchPaths << QDir::homePath() + "/.local/share/icons"
                       << "/usr/share/icons"
                       << "/usr/local/share/icons";
    qDebug() << "[IconProvider] init, theme=" << s_themeName;
}

QImage IconThemeImageProvider::requestImage(const QString &id,
                                             QSize *realSize,
                                             const QSize &requestedSize)
{
    int w = requestedSize.width()  > 0 ? requestedSize.width()  : 32;
    int h = requestedSize.height() > 0 ? requestedSize.height() : 32;
    if (realSize) *realSize = QSize(w, h);

    // qrc: 路径不是图标名，直接返回透明（QML 那边应该用 source 而非 image://icontheme）
    if (id.startsWith("qrc:") || id.startsWith(":")) {
        QImage img(id.mid(id.startsWith("qrc") ? 3 : 0));
        if (!img.isNull())
            return img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QImage placeholder(w, h, QImage::Format_ARGB32_Premultiplied);
        placeholder.fill(Qt::transparent);
        return placeholder;
    }

    // 本地绝对路径
    if (id.startsWith('/')) {
        QImage img(id);
        if (!img.isNull())
            return img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QString path = findIconFile(id, w);
    QImage img = path.isEmpty() ? QImage() : QImage(path);
    if (img.isNull()) {
        QImage placeholder(w, h, QImage::Format_ARGB32_Premultiplied);
        placeholder.fill(Qt::transparent);
        return placeholder;
    }
    return img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
