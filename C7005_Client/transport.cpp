#include "transport.h"
#include <QDebug>
#include <QNetworkDatagram>

Transport::Transport(QString ip, unsigned short port, QString destIP, unsigned short destPort, unsigned short windowSize, QObject *parent)
    : QObject(parent),
      destPort(destPort),
      sendTimer(new QTimer()),
      windowSize(windowSize),
      sendWindow(nullptr)

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
        windowEnd = &sendWindow[windowSize/2];
        qDebug() << windowEnd - windowStart;
    }
    connect(this->parent(), SIGNAL(readyToSend(QFile*)), this, SLOT(sendURGPack(QFile*)));
    connect(sock, SIGNAL(readyRead()), this, SLOT(receiveURG()));
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
    QString filename =  file->fileName().mid(file->fileName().lastIndexOf('/') + 1, -1);
    DataPacket urgPack;
    memset(&urgPack,0, sizeof(DataPacket));
    urgPack.packetType = URG;
    memcpy(urgPack.data, qPrintable(filename), static_cast<size_t>(filename.size()));
    sock->writeDatagram(reinterpret_cast<char *>(&urgPack), sizeof(urgPack), *destAddress, destPort);

    disconnect(sock, SIGNAL(readyRead()), this, SLOT(receiveURG()));
    connect(sock, SIGNAL(readyRead()), this, SLOT(waitForACK()));
}

void Transport::receiveURG()
{
    char buffer[512];
    while(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, 512);
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

void Transport::waitForACK()
{
    char buffer[HEADERLEN];
    while(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, HEADERLEN);
        ControlPacket* data = reinterpret_cast<ControlPacket *>(buffer);
        if(data->packetType == ACK)
        {
            qDebug() << "recived: ack";
            //ControlPacket ack;
            //ack.packetType = ACK;
            //sock->writeDatagram(reinterpret_cast<char *>(&ack), sizeof(ControlPacket), *destAddress, destPort);
        }


    }
}

void Transport::sendNPackets(QFile *file)
{
    while(windowEnd - windowStart < windowSize/2 - 1)
    {

    }
}
