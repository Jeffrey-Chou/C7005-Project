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

    void setFile(QFile * file);


signals:
    void beginContention();

public slots:
    void sendURGPack(bool);
    void recvURG();
    void recvURGResponse();
    void recvData();
    bool sendPacket();
    void sendNPackets();
    void recvDataAck();


    void sendTimeOut();
    void recvTimeOut();

    void contention();
    void contentionTimeOut();

private:
    QUdpSocket *sock;
    QHostAddress *destAddress;
    unsigned short destPort;

    QTimer *sendTimer, *receiveTimer, *contendTimer;
    QQueue<QTime> timeQueue;

    unsigned short windowSize;
    DataPacket **sendWindow, **windowStart, **windowEnd;

    QFile *sendFile;
    bool transferMode;

    void retransmit();
    int expectSeq;

};

#endif // TRANSPORT_H
