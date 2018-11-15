#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <QObject>
#include <QUdpSocket>
#include <QFile>
#include <QTimer>
#include <QQueue>
#include <QTime>

#include "packet.h"

#define TIMEOUT 3000

class Transport : public QObject
{
    Q_OBJECT
public:
    //explicit Transport(QObject *parent = nullptr);
    explicit Transport(QString ip, unsigned short port,
                       QString destIP, unsigned short destPort, unsigned short windowSize, QObject *parent = nullptr);
    ~Transport();




signals:

public slots:
    void sendURGPack(QFile *file);
    void receiveURG();
    void waitForURGResponse();

    void sendPacket();
    void sendNPackets();
    void recvDataAck();


    void sendTimeOut();

private:
    QUdpSocket *sock;
    QHostAddress *destAddress;
    unsigned short destPort;

    QTimer *sendTimer, *receiveTimer;
    QQueue<QTime> timeQueue;

    unsigned short windowSize;
    DataPacket **sendWindow, **windowStart, **windowEnd;

    QFile *sendFile;
    bool transferMode;

    void retransmit();

};

#endif // TRANSPORT_H
