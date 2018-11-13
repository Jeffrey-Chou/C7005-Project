#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QFile>

#include "transport.h"

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
    void readyToSend(QFile *);

public slots:
    void loadFile();
    void send();
    void configureTransport();

private:
    Ui::Client *ui;
    QFile *sendFile;
    Transport *transport;
    void loadConfig();
};

#endif // CLIENT_H
