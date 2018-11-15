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
      transferMode(false)

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
    connect(sock, SIGNAL(readyRead()), this, SLOT(receiveURG()));
    connect(sendTimer, SIGNAL(timeout()), this, SLOT(sendTimeOut()));
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
    disconnect(sock, SIGNAL(readyRead()), this, SLOT(receiveURG()));
    connect(sock, SIGNAL(readyRead()), this, SLOT(waitForURGResponse()));
    qDebug() << "sending urg";
}

void Transport::receiveURG()
{
    char buffer[PAYLOADLEN + HEADERLEN];
    while(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, PAYLOADLEN + HEADERLEN);
        DataPacket* data = reinterpret_cast<DataPacket *>(buffer);
        if(data->packetType == URG)
        {
            qDebug() << "recived: " << data->data;
            ControlPacket ack;
            ack.packetType = ACK;
            sock->writeDatagram(reinterpret_cast<char *>(&ack), sizeof(ControlPacket), *destAddress, destPort);
        }


    }
}

void Transport::waitForURGResponse()
{
    char buffer[HEADERLEN];
    while(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, HEADERLEN);
        ControlPacket* data = reinterpret_cast<ControlPacket *>(buffer);
        if(data->packetType == ACK)
        {
            qDebug() << "recived: ack";
            sendTimer->stop();
            transferMode = true;
            sendNPackets();
        }


    }
}

void Transport::sendPacket()
{
    if((windowEnd - sendWindow < windowSize - 1) && !sendFile->atEnd())
    {
        DataPacket *data = new DataPacket;
        data->packetType = DATA;
        data->seqNum = static_cast<int>(windowEnd - sendWindow);
        qDebug() << "seq num:" << data->seqNum;
        data->windowSize = 8;
        data->ackNum = 69;
        sendFile->read(data->data, PAYLOADLEN);
        if(sendFile->atEnd())
        {
            data->packetType += FIN;
        }
        sock->writeDatagram(reinterpret_cast<char *>(data), sizeof(DataPacket), *destAddress, destPort);
        *windowEnd++ = data;
        qDebug() << "data: " << data->data;
    }
}

void Transport::sendNPackets()
{
    int n = windowSize/2 - (windowSize % 2 == 0 ? 1 : 0);
    while(windowEnd - windowStart <= n )
    {

        sendPacket();
    }
    sendTimer->start(TIMEOUT);
}

void Transport::recvDataAck()
{
    char buffer[HEADERLEN];
    while(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, HEADERLEN);
        ControlPacket* con = reinterpret_cast<ControlPacket *>(buffer);
        if(con->packetType == ACK && con->ackNum == (*windowStart)->ackNum+1)
        {
            timeQueue.dequeue();
            delete *windowStart++;
            sendPacket();
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
