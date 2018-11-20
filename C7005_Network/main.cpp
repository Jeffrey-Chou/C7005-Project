#include "network.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Network w;
    w.show();

    return a.exec();
}
