#include "dockadaptor.h"
#include "mainwindow.h"

DockAdaptor::DockAdaptor(MainWindow *parent)
    : QDBusAbstractAdaptor(parent)
    , m_mainWindow(parent)
{
    setAutoRelaySignals(true);
}

void DockAdaptor::add(const QString &desktop)
{
    m_mainWindow->add(desktop);
}

void DockAdaptor::remove(const QString &desktop)
{
    m_mainWindow->remove(desktop);
}

bool DockAdaptor::pinned(const QString &desktop)
{
    return m_mainWindow->pinned(desktop);
}
