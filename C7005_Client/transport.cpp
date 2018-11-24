#include "transport.h"
#include <QDebug>
#include <QNetworkDatagram>

Transport::Transport(QString ip, unsigned short port, QString destIP, unsigned short destPort, unsigned short windowSize, QObject *parent)
    : QObject(parent),
      destPort(destPort),
      sendTimer(new QTimer(this)),
      receiveTimer(new QTimer(this)),
      contendTimer(new QTimer(this)),
      windowSize(windowSize),
      sendWindow(nullptr),
      sendFile(nullptr),
      recvFile(new QFile()),
      transferMode(false),
      sendingMachine(false),
      expectSeq(0),
      sendTOCount(0),
      recvTOCount(0)

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
    //debug = new TransportDebug(windowSize, this);
    sendTimer->setSingleShot(true);
    receiveTimer->setSingleShot(true);
    contendTimer->setSingleShot(true);

    connect(this->parent(), SIGNAL(readyToSend(bool)), this, SLOT(sendURGPack(bool)));
    connect(sock, SIGNAL(readyRead()), this, SLOT(recvPacket()));
    connect(sendTimer, SIGNAL(timeout()), this, SLOT(sendTimeOut()));
    connect(receiveTimer, SIGNAL(timeout()), this, SLOT(recvTimeOut()));
    connect(contendTimer, SIGNAL(timeout()), this , SLOT(contentionTimeOut()));
    connect(this, SIGNAL(beginContention()), this, SLOT(contention()), Qt::QueuedConnection);
    //connect(sendTimer, )

}

Transport::~Transport()
{
    sock->close();
    if(sendWindow)
        delete[] sendWindow;
}

void Transport::setFile(QFile *file)
{
    sendFile = file;
}

void Transport::sendURGPack(bool fromClient)
{
    //sendFile = file;
    QString filename =  sendFile->fileName().mid(sendFile->fileName().lastIndexOf('/') + 1, -1);
    DataPacket urgPack;
    memset(&urgPack,0, sizeof(DataPacket));
    urgPack.packetType = URG;
    urgPack.windowSize = windowSize;
    sendingMachine = true;
    memcpy(urgPack.data, qPrintable(filename), static_cast<size_t>(filename.size()));
    sock->writeDatagram(reinterpret_cast<char *>(&urgPack), sizeof(urgPack), *destAddress, destPort);
    emit(packetSent(-1, URG));
    if(fromClient)
    {
        sendTimer->start(TIMEOUT);
    }
    //disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvURG()));
    //connect(sock, SIGNAL(readyRead()), this, SLOT(recvURGResponse()));
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
            contendTimer->stop();
            transferMode = false;
            if(!recvFile->isOpen())
            {
                QString name(data->data);

                recvFile->setFileName(name.insert(0,'1'));

                recvFile->open(QIODevice::WriteOnly | QIODevice::Text);
            }
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

bool Transport::sendPacket()
{
    bool done = false;
    if((windowEnd - sendWindow < windowSize) && !sendFile->atEnd())
    {
        DataPacket *data = new DataPacket;
        data->packetType = DATA;
        memset(data->data, 0, PAYLOADLEN);
        data->seqNum = static_cast<int>(windowEnd - sendWindow);
        data->windowSize = windowSize;
        data->ackNum = 0;
        qDebug() << "sending packet" << data->seqNum;
        sendFile->read(data->data, PAYLOADLEN);
        if(sendFile->atEnd())
        {
            data->packetType += FIN;
            done = true;
        }
        sock->writeDatagram(reinterpret_cast<char *>(data), sizeof(DataPacket), *destAddress, destPort);
        timeQueue.enqueue(QTime::currentTime());
        emit(packetSent(static_cast<int>(windowEnd - sendWindow), data->packetType));
        *windowEnd++ = data;


    }
    return done;
}

void Transport::sendNPackets()
{
    qDebug() << "sending n packets";
    int n = windowSize/2;
    while(windowEnd - windowStart < n )
    {

        if(sendPacket())
        {
            break;
        }
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

        if(data->packetType == ACK)
        {
            disconnect(sock, SIGNAL(readyReady()), this, SLOT(recvData()));
            connect(sock, SIGNAL(readyRead()), this, SLOT(recvDataAck()));
            transferMode = true;
            sendNPackets();
            break;
        }


        if((data->packetType & DATA) == DATA && data->seqNum == expectSeq)
        {
            receiveTimer->stop();
            recvFile->write(data->data, PAYLOADLEN);
            ControlPacket ack;
            ack.packetType = ACK;
            ack.ackNum = ++expectSeq;
            qDebug() << "           expected seq: " << expectSeq;
            sock->writeDatagram(reinterpret_cast<char *>(&ack), sizeof(ControlPacket), *destAddress, destPort);
            if(expectSeq == data->windowSize)
            {
                if((data->packetType & FIN) == FIN)
                {
                    recvFile->close();
                }
                expectSeq = 0;
                emit(beginContention());
                break;
            }
            if((data->packetType & FIN) == FIN )
            {
                recvFile->close();
                expectSeq = 0;
                emit(beginContention());
            }
            else
            {
                receiveTimer->start(TIMEOUT);
            }
        }


    }
}

void Transport::recvDataAck()
{
    char buffer[HEADERLEN + PAYLOADLEN];
    while(sock->hasPendingDatagrams())
    {
        sock->readDatagram(buffer, HEADERLEN + PAYLOADLEN);
        ControlPacket* con = reinterpret_cast<ControlPacket *>(buffer);
        qDebug() << "wating for ack " << (*windowStart)->seqNum+1;
        qDebug() << "       received ack " << con->ackNum;
        if(con->packetType == ACK )
        {
            if(con->ackNum >= (*windowStart)->seqNum+1)
            {
                qDebug() << "in recvDataAck got " << con->ackNum;
                sendTimer->stop();
                emit(packetRecv(con->ackNum, ACK));
                delete *windowStart;
                ++windowStart;
                timeQueue.dequeue();
                if(windowStart == windowEnd)
                {
                    // TODO add weird logic here
                    windowStart = windowEnd = sendWindow;
                    emit(beginContention());
                    break;
                }
                sendPacket();
                sendTimer->start(TIMEOUT - timeQueue.head().msecsTo(QTime::currentTime()));
            }
            if(con->ackNum == 0 && windowEnd - sendWindow == windowSize)
            {
                sendTimer->stop();
                timeQueue.clear();
                int start = static_cast<int>(windowStart - sendWindow);
                int end = static_cast<int>(windowEnd - sendWindow);
                qDebug () << "retransmiting from:" << start << " to " << end;
                for(int i = start; i < end; ++i)
                {
                    delete sendWindow[i];
                }
                windowStart = windowEnd = sendWindow;
                emit(beginReset());
                sendNPackets();
            }
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
        sendURGPack(true);
    }
}

void Transport::recvTimeOut()
{
    qDebug() << "recv timeout sending ack" << expectSeq;
    sendAckPack(expectSeq);
    receiveTimer->start(TIMEOUT);
}

void Transport::retransmit()
{
    timeQueue.clear();
    int start = static_cast<int>(windowStart - sendWindow);
    int end = static_cast<int>(windowEnd - sendWindow);
    qDebug () << "retransmiting from:" << start << " to " << end;
    for(int i = start; i < end; ++i)
    {
        sock->writeDatagram(reinterpret_cast<char *>(sendWindow[i]), sizeof(DataPacket), *destAddress, destPort);
        timeQueue.enqueue(QTime::currentTime());
    }
    sendTimer->start(TIMEOUT);
    if(sendFile->atEnd())
    {
        ++sendTOCount;
        if(sendTOCount > 5)
        {
            emit(beginContention());
        }
    }

}

void Transport::contention()
{
    qDebug() <<"contention has begun";
    sendTimer->stop();
    receiveTimer->stop();
    if(transferMode)
    {
        //disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvDataAck()));
        //connect(sock, SIGNAL(readyRead()), this, SLOT(recvURG()));
        contendTimer->start(TIMEOUT + 1500);
    }
    else
    {
        //if receiver has data to send, send request
        //else wait for more data
        //disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvData()));
        //connect(sock, SIGNAL(readyRead()), this, SLOT(recvURGResponse()));
        if(sendFile != nullptr && !sendFile->atEnd())
        {
            sendingMachine = true;
            for(int i = 0; i < windowSize/2; ++i)
                sendURGPack(false);
        }
        contendTimer->start(TIMEOUT);
    }
}

void Transport::contentionTimeOut()
{
    qDebug() << "contention timeout";
    if(transferMode)
    {
        if(!sendFile->atEnd())
        {   qDebug() << "starting to send n packets again";
            emit(beginReset());
            //disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvURG()));
            //connect(sock, SIGNAL(readyRead()), this, SLOT(recvDataAck()));
            sendNPackets();
        }
    }
    else
    {
        //disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvURGResponse()));
        //connect(sock, SIGNAL(readyRead()), this, SLOT(recvData()));
        qDebug() << "starting recv timer again";
        receiveTimer->start(TIMEOUT);
        sendingMachine = false;
    }

    if(sendFile == nullptr || sendFile->atEnd())
    {
        if(!recvFile->isOpen())
        {
            qDebug() << "ready to close everything";
            //disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvData()));
            //disconnect(sock, SIGNAL(readyRead()), this, SLOT(recvDataAck()));
            //connect(sock, SIGNAL(readyRead()), this, SLOT(recvURG()));
            receiveTimer->stop();
            sendTimer->stop();
        }
    }
}

void Transport::recvPacket()
{
    char buffer[HEADERLEN + PAYLOADLEN];
    ControlPacket * con = nullptr;
    DataPacket *data = nullptr;
    while(sock->hasPendingDatagrams())
    {
        qint64 read = sock->readDatagram(buffer, HEADERLEN + PAYLOADLEN);
        if(read == HEADERLEN)
        {
            con = reinterpret_cast<ControlPacket *>(buffer);
            data = nullptr;
            if(sendingMachine)
            {
                senderHandleControl(con);
            }
            else
            {
                receiverHandleControl(con);
            }
        }
        else
        {
            data = reinterpret_cast<DataPacket *>(buffer);
            con = nullptr;
            if(sendingMachine)
            {
                senderHandleData(data);
            }
            else
            {
                receiverHandleData(data);
            }
        }

    }
}

void Transport::senderHandleControl(ControlPacket *con)
{
    if(transferMode)
    {
        qDebug() << "transfer mode recived: ack" << con->ackNum;
        if(con->ackNum >= (*windowStart)->seqNum+1)
        {
            qDebug() << "in recvDataAck got " << con->ackNum;
            sendTimer->stop();
            emit(packetRecv(con->ackNum, ACK));
            delete *windowStart;
            ++windowStart;
            timeQueue.dequeue();
            if(windowStart == windowEnd)
            {
                // TODO add weird logic here
                windowStart = windowEnd = sendWindow;
                emit(beginContention());
                return;
            }
            sendPacket();
            int time = TIMEOUT - timeQueue.head().msecsTo(QTime::currentTime());
            if(time < 0)
                time = 0;
            qDebug() << "tiemout time is " << time;
            sendTimer->start(time);
        }
        if(con->ackNum == 0 && windowEnd - sendWindow == windowSize)
        {
            sendTimer->stop();
            timeQueue.clear();
            int start = static_cast<int>(windowStart - sendWindow);
            int end = static_cast<int>(windowEnd - sendWindow);
            qDebug () << "got ack 0 while ";
            for(int i = start; i < end; ++i)
            {
                delete sendWindow[i];
            }
            windowStart = windowEnd = sendWindow;
            emit(beginReset());
            sendNPackets();
        }
    }
    else
    {
        qDebug() << "recived: urg ack";
        sendTimer->stop();
        contendTimer->stop();
        transferMode = true;
        sendNPackets();
    }
}

void Transport::senderHandleData(DataPacket *data)
{
    if(data->packetType == URG )
    {
        qDebug() << "sender received urg";
        contendTimer->stop();
        sendTimer->stop();
        sendingMachine = transferMode = false;
        windowStart = windowEnd = sendWindow;
        if(!recvFile->isOpen())
        {
            QString name(data->data);
            recvFile->setFileName(name.insert(0,'1'));
            recvFile->open(QIODevice::WriteOnly | QIODevice::Text);
        }
        sendAckPack(0);
        receiveTimer->start(TIMEOUT);
    }
}


void Transport::receiverHandleControl(ControlPacket *con)
{
    if(con->packetType == ACK)
    {
        qDebug() << "receiver got ack";
        receiveTimer->stop();
        sendingMachine = true;
        sendNPackets();
    }
}

void Transport::receiverHandleData(DataPacket *data)
{
    if(data->packetType == URG)
    {
        qDebug() << "recieved urg";
        contendTimer->stop();

        if(!recvFile->isOpen())
        {
            QString name(data->data);
            recvFile->setFileName(name.insert(0,'1'));
            recvFile->open(QIODevice::WriteOnly | QIODevice::Text);
        }
        sendAckPack(0);
        receiveTimer->start(TIMEOUT);
    }
    qDebug() << "received data: " <<data->seqNum;
    if((data->packetType & DATA) == DATA && data->seqNum == expectSeq)
    {
        receiveTimer->stop();
        recvFile->write(data->data, PAYLOADLEN);
        sendAckPack(++expectSeq);
        qDebug() << "           expected seq: " << expectSeq;
        if(expectSeq == data->windowSize || (data->packetType & FIN) == FIN)
        {
            if((data->packetType & FIN) == FIN)
            {
                recvFile->close();
            }
            expectSeq = 0;
            emit(beginContention());
            return;
        }
        else
        {
            receiveTimer->start(TIMEOUT);
        }
    }
}

void Transport::sendAckPack(int ackNum)
{
    qDebug() << "sending ack";
    ControlPacket ack;
    ack.packetType = ACK;
    ack.ackNum = ackNum;
    sock->writeDatagram(reinterpret_cast<char *>(&ack), sizeof(ControlPacket), *destAddress, destPort);
}
