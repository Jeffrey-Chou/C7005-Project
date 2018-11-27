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
    void openDebug(QString, unsigned short);
    void beginContention();
    void packetSent(int,int);
    void packetRecv(int,int);
    void retransmit(int, int, int);
    void beginReset();
    void finished();

public slots:
    void sendURGPack(bool);



    bool sendPacket();
    void sendNPackets();



    void sendTimeOut();
    void recvTimeOut();

    void contention();
    void contentionTimeOut();

    void recvPacket();

private:
    QUdpSocket *sock;
    QHostAddress *destAddress;
    unsigned short destPort;

    QTimer *sendTimer, *receiveTimer, *contendTimer;
    QQueue<QTime> timeQueue;

    unsigned short windowSize;
    DataPacket **sendWindow, **windowStart, **windowEnd;

    QFile *sendFile, *recvFile;
    bool transferMode, sendingMachine;

    void retransmit();
    int expectSeq, sendTOCount, recvTOCount;

    //TransportDebug *debug;
    void senderHandleControl(ControlPacket *);
    void senderHandleData(DataPacket *);
    void receiverHandleControl(ControlPacket *);
    void receiverHandleData(DataPacket *);

    void  sendAckPack(int ackNum);

};

#endif // TRANSPORT_H
