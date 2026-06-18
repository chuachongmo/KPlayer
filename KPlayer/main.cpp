#include "KPlayerUI.h"
#include <QtWidgets/QApplication>




int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KPlayerUI window;
    window.show();
    return app.exec();
}
