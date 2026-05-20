#include "AccessoryWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Codex");
    QCoreApplication::setApplicationName("ClaudeUsageMonitor");

    AccessoryWindow window;
    window.show();

    return app.exec();
}
