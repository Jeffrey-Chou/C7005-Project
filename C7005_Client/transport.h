#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <QObject>
#include <QUdpSocket>
#include <QFile>
#include <QTimer>
#include <QQueue>
#include <QTime>

#include "packet.h"

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

    void waitForACK();

    void sendNPackets(QFile *file);

private:
    QUdpSocket *sock;
    QHostAddress *destAddress;
    unsigned short destPort;

    QTimer *sendTimer, *receiveTimer;
    QQueue<QTime> timeQueue;

    unsigned short windowSize;
    DataPacket **sendWindow, **windowStart, **windowEnd;

};

#endif // TRANSPORT_H
