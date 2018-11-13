#include "transport.h"
#include <QDebug>
#include <QNetworkDatagram>

Transport::Transport(QString ip, unsigned short port, QString destIP, unsigned short destPort,QObject *parent)
    : QObject(parent),
      destPort(destPort)

{
    sock = new QUdpSocket(parent);
    bool success = sock->bind(QHostAddress(ip), port);
    qDebug() << "socket binded: " << success;
    destAddress = new QHostAddress(destIP);
    connect(this->parent(), SIGNAL(readyToSend(QFile*)), this, SLOT(send(QFile*)));
    connect(sock, SIGNAL(readyRead()), this, SLOT(receive()));

}

Transport::~Transport()
{
    sock->close();
}

void Transport::send(QFile *file)
{
    qDebug() << "entered send";
    if(!file->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "file didn't open";
        return;
    }

    while(!file->atEnd())
    {
        QByteArray data = file->read(16);
        sock->writeDatagram(data, *destAddress, destPort);
    }
    file->close();
}

void Transport::receive()
{
    while(sock->hasPendingDatagrams())
    {
        QNetworkDatagram data = sock->receiveDatagram();
        qDebug() << data.data();
    }
}
