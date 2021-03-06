#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QFile>

#include "transport.h"
#include "transportdebug.h"

namespace Ui {
class Client;
}

class Client : public QMainWindow
{
    Q_OBJECT

public:
    explicit Client(QWidget *parent = nullptr);
    ~Client();

signals:
    void readyToSend(bool);

public slots:
    void loadFile();
    void send();
    void configureTransport();
    void openDebugWindow(QString, unsigned short);

private:
    Ui::Client *ui;
    QFile *sendFile;
    Transport *transport;
    TransportDebug *debug;
    void loadConfig();
};

#endif // CLIENT_H
