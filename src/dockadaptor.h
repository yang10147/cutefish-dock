#pragma once

#include <QDBusAbstractAdaptor>
#include <QString>

class MainWindow;

class DockAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.Dock")

public:
    explicit DockAdaptor(MainWindow *parent);

public slots:
    void add(const QString &desktop);
    void remove(const QString &desktop);
    bool pinned(const QString &desktop);

private:
    MainWindow *m_mainWindow;
};
