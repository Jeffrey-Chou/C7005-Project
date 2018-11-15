#include "transport.h"
#include <QDebug>
#include <QNetworkDatagram>

Transport::Transport(QString ip, unsigned short port, QString destIP, unsigned short destPort, unsigned short windowSize, QObject *parent)
    : QObject(parent),
      destPort(destPort),
      sendTimer(new QTimer(this)),
      receiveTimer(new QTimer(this)),
      windowSize(windowSize),
      sendWindow(nullptr),
      transferMode(false),
      expectSeq(0)

{
    sock = new QUdpSocket(parent);
    bool success = sock->bind(QHostAddress(ip), port);
    qDebug() << "socket binded: " << success;
    destAddress = new QHostAddress(destIP);
    qDebug() << "send window" << windowSize;
    if(windowSize > 0)
    {
        sendWindow = new DataPacket*[windowSize];
        windowStart = windowEnd = sendWindow;
        //windowEnd = &sendWindow[windowSize/2];
        qDebug() << windowEnd - windowStart;
    }

    connect(this->parent(), SIGNAL(readyToSend(QFile*)), this, SLOT(sendURGPack(QFile*)));
    connect(sock, SIGNAL(readyRead()), this, SLOT(recvURG()));
    connect(sendTimer, SIGNAL(timeout()), this, SLOT(sendTimeOut()));
    connect(receiveTimer, SIGNAL(timeout()), this, SLOT(recvTimeOut()));
    //connect(sendTimer, )

}

Transport::~Transport()
{
    sock->close();
    if(sendWindow)
        delete[] sendWindow;
}

void Transport::sendURGPack(QFile *file)
{
    sendFile = file;
    QString filename =  file->fileName().mid(file->fileName().lastIndexOf('/') + 1, -1);
    DataPacket urgPack;
    memset(&urgPack,0, sizeof(DataPacket));
    urgPack.packetType = URG;
    urgPack.windowSize = windowSize;
    memcpy(urgPack.data, qPrintable(filename), static_cast<size_t>(filename.size()));
    sock->writeDatagram(reinterpret_cast<char *>(&urgPack), sizeof(urgPack), *destAddress, destPort);
    sendTimer->start(TIMEOUT);
    disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvURG()));
    connect(sock, SIGNAL(readyRead()), this, SLOT(recvURGResponse()));
    qDebug() << "sending urg";
}

void Transport::recvURG()
{
    char buffer[PAYLOADLEN + HEADERLEN];
    while(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, PAYLOADLEN + HEADERLEN);
        DataPacket* data = reinterpret_cast<DataPacket *>(buffer);
        if(data->packetType == URG)
        {
            disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvURG()));
            connect(sock, SIGNAL(readyRead()), this, SLOT(recvData()));
            ControlPacket ack;
            ack.packetType = ACK;
            sock->writeDatagram(reinterpret_cast<char *>(&ack), sizeof(ControlPacket), *destAddress, destPort);
            receiveTimer->start(TIMEOUT);
        }


    }
}

void Transport::recvURGResponse()
{
    char buffer[HEADERLEN];
    if(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, HEADERLEN);
        ControlPacket* data = reinterpret_cast<ControlPacket *>(buffer);
        if(data->packetType == ACK)
        {
            qDebug() << "recived: ack";
            sendTimer->stop();
            disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvURGResponse()));
            connect(sock, SIGNAL(readyRead()), this, SLOT(recvDataAck()));
            transferMode = true;
            sendNPackets();
        }


    }
}

void Transport::sendPacket()
{
    if((windowEnd - sendWindow < windowSize) && !sendFile->atEnd())
    {
        DataPacket *data = new DataPacket;
        data->packetType = DATA;
        data->seqNum = static_cast<int>(windowEnd - sendWindow);
        data->windowSize = windowSize;
        data->ackNum = 0;
        sendFile->read(data->data, PAYLOADLEN);
        if(sendFile->atEnd())
        {
            data->packetType += FIN;
        }
        sock->writeDatagram(reinterpret_cast<char *>(data), sizeof(DataPacket), *destAddress, destPort);
        timeQueue.enqueue(QTime::currentTime());
        *windowEnd++ = data;

        qDebug() << "data: " << data->data;
    }
}

void Transport::sendNPackets()
{
    int n = windowSize/2;
    while(windowEnd - windowStart < n )
    {

        sendPacket();
    }
    sendTimer->start(TIMEOUT);
}

void Transport::recvData()
{
    char buffer[PAYLOADLEN + HEADERLEN];
    while(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, PAYLOADLEN + HEADERLEN);
        DataPacket *data = reinterpret_cast<DataPacket *>(&buffer);
        qDebug() << "received seq: " << data->seqNum;
        if(data->packetType == DATA && data->seqNum == expectSeq)
        {
            receiveTimer->stop();
            ControlPacket ack;
            ack.packetType = ACK;
            ack.ackNum = ++expectSeq;
            qDebug() << "expected seq: " << expectSeq;
            sock->writeDatagram(reinterpret_cast<char *>(&ack), sizeof(ControlPacket), *destAddress, destPort);
            receiveTimer->start(TIMEOUT);
        }
    }
}

void Transport::recvDataAck()
{
    char buffer[HEADERLEN];
    while(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, HEADERLEN);
        ControlPacket* con = reinterpret_cast<ControlPacket *>(buffer);
        qDebug() << "wating for ack " << (*windowStart)->seqNum+1;
        if(con->packetType == ACK && con->ackNum == (*windowStart)->seqNum+1)
        {
            qDebug() << "in recvDataAck";
            sendTimer->stop();
            timeQueue.dequeue();
            delete *windowStart;
            ++windowStart;
            sendPacket();
            sendTimer->start(TIMEOUT - timeQueue.head().msecsTo(QTime::currentTime()));
        }


    }
}

void Transport::sendTimeOut()
{
    qDebug() << "send timeout";
    if(transferMode)
    {
        retransmit();
    }
    else
    {
        sendURGPack(sendFile);
    }
}

void Transport::recvTimeOut()
{
    qDebug() << "recv timeout";
    ControlPacket ack;
    ack.packetType = ACK;
    ack.ackNum = expectSeq;
    sock->writeDatagram(reinterpret_cast<char *>(&ack), sizeof(ControlPacket), *destAddress, destPort);
    receiveTimer->start(TIMEOUT);
}

void Transport::retransmit()
{
    timeQueue.clear();
    int start = static_cast<int>(windowStart - sendWindow);
    int end = static_cast<int>(windowEnd - sendWindow);
    for(int i = start; i <= end; ++i)
    {
        sock->writeDatagram(reinterpret_cast<char *>(sendWindow[i]), sizeof(DataPacket), *destAddress, destPort);
        timeQueue.enqueue(QTime::currentTime());
    }
    sendTimer->start(TIMEOUT);
}
