#ifndef NETWORK_H
#define NETWORK_H

#include <QMainWindow>
#include <QFile>
#include <QUdpSocket>

#include <stdlib.h>
#include <time.h>

#define BUFFSIZE 512

namespace Ui {
class Network;
}

class Network : public QMainWindow
{
    Q_OBJECT

public:
    explicit Network(QWidget *parent = nullptr);
    ~Network();

    bool forwardPacket();

public slots:
    void handleSliderChange(int);
    void listen();
    void sendToMac01();
    void sendToMac02();

private:
    Ui::Network *ui;
    QUdpSocket *fromMac01, *fromMac02;
    QHostAddress mac01IP, mac02IP;
    unsigned short mac01Port, mac02Port;

    char mac01Buffer[BUFFSIZE];
    char mac02Buffer[BUFFSIZE];

    int errorRate;
    int delay;

    bool isListen;




    void loadConfig();
};

#endif // NETWORK_H
