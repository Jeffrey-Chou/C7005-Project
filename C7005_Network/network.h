#ifndef NETWORK_H
#define NETWORK_H

#include <QMainWindow>
#include <QFile>
#include <QUdpSocket>
#include <QTimer>
#include <QQueue>

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
    void toMac01DelayOver();
    void toMac02DelayOver();

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
    QTimer toMac01Timer, toMac02Timer;
    QQueue<char *> toMac01Queue, toMac02Queue;
    QQueue<long long> toMac01PacketSize, toMac02PacketSize;
    unsigned int toMac01Forwarded, toMac01Dropped, toMac02Forwarded, toMac02Dropped;




    void loadConfig();
};

#endif // NETWORK_H
