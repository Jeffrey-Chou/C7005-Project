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
    }

    sendTimer->setSingleShot(true);
    receiveTimer->setSingleShot(true);
    contendTimer->setSingleShot(true);

    connect(this->parent(), SIGNAL(readyToSend(bool)), this, SLOT(sendURGPack(bool)));
    connect(sock, SIGNAL(readyRead()), this, SLOT(recvPacket()));
    connect(sendTimer, SIGNAL(timeout()), this, SLOT(sendTimeOut()));
    connect(receiveTimer, SIGNAL(timeout()), this, SLOT(recvTimeOut()));
    connect(contendTimer, SIGNAL(timeout()), this , SLOT(contentionTimeOut()));
    connect(this, SIGNAL(beginContention()), this, SLOT(contention()), Qt::QueuedConnection);

}

Transport::~Transport()
{
    sock->close();
    if(sendWindow)
        delete[] sendWindow;
}

// Sets the sendfile pointer to the file pointer from the client
void Transport::setFile(QFile *file)
{
    sendFile = file;
}

// Creates an URG packet and sends it. An URG packet is a DATA packet with the file name as the payload
void Transport::sendURGPack(bool fromClient)
{
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
    qDebug() << "sending urg";
}


// Generates and sends a packet if there is still space in the send window. Returns true if not able to send anymore
bool Transport::sendPacket()
{
    bool done = true;
    if((windowEnd - sendWindow < windowSize) && !sendFile->atEnd())
    {
        done = false;
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

// Sends multiple packets, up to half of the window size.
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



// Called when the send timer elapses. If in transfer mode, retransmit the current window, else resend urg.
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

// Called when receive timer elaspes. Resends an ack with the expected seq number
void Transport::recvTimeOut()
{
    qDebug() << "recv timeout sending ack" << expectSeq;
    sendAckPack(expectSeq);
    emit(retransmit(expectSeq, expectSeq, ACK));
    receiveTimer->start(TIMEOUT);
}

// Retransmits the current send window from window start to window end.
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
    emit(retransmit(start, end, DATA));
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

// Called when the begin contention signal is emited. If the client was in transfer mode, wait for contention.
// Else, send a URG request if there is file to send and wait.
void Transport::contention()
{
    qDebug() <<"contention has begun";
    sendTimer->stop();
    receiveTimer->stop();
    if(transferMode)
    {

        contendTimer->start(TIMEOUT + 1500);
    }
    else
    {

        if(sendFile != nullptr && !sendFile->atEnd())
        {
            sendingMachine = true;
            for(int i = 0; i < windowSize/2; ++i)
                sendURGPack(false);
        }
        contendTimer->start(TIMEOUT);
    }
}

// Called when the contention timer elapses. If the client was in transfer mode, send half the window size of packets,
// else, start the receive timer again. If the sendfile is not set or at the end and the receive file is closed then
// everything is finished.
void Transport::contentionTimeOut()
{
    qDebug() << "contention timeout";
    if(transferMode)
    {
        if(!sendFile->atEnd())
        {   qDebug() << "starting to send n packets again";
            emit(beginReset());
            sendNPackets();
        }
    }
    else
    {
        qDebug() << "starting recv timer again";
        receiveTimer->start(TIMEOUT);
        sendingMachine = false;
    }

    if(sendFile == nullptr || sendFile->atEnd())
    {
        if(!recvFile->isOpen())
        {
            qDebug() << "ready to close everything";
            receiveTimer->stop();
            sendTimer->stop();
            emit(finished());
        }
    }
}

// Called when the socket receives data. Depending on the size, the packet is a DATA packet or a Control packet.
// Performs different tasks if the client is a sending machine or not.
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

// Handles control packets if the client is the sender. If an ack is received and the sender is not transfering, start
// transfering. If transfering and the expected ack is received, send more packets.
void Transport::senderHandleControl(ControlPacket *con)
{
    emit(packetRecv(con->ackNum, ACK));
    if(transferMode)
    {
        qDebug() << "transfer mode recived: ack" << con->ackNum;
        if(con->ackNum >= (*windowStart)->seqNum+1)
        {
            qDebug() << "in recvDataAck got " << con->ackNum;
            sendTimer->stop();

            while(windowStart != windowEnd && (*windowStart)->seqNum+1 <= con->ackNum)
            {
                delete *windowStart;
                ++windowStart;
                timeQueue.dequeue();
            }


            sendNPackets();
            if(windowStart == windowEnd)
            {
                windowStart = windowEnd = sendWindow;
                emit(beginContention());
                return;
            }

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

// If the sender receives an URG packet, start receiving data.
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
        emit(packetRecv(-1, URG));
        sendAckPack(0);
        emit(packetSent(0, ACK));

        receiveTimer->start(TIMEOUT);
    }
}

// If the receiver receives an ACK packet, start sending packets
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

// If the receiver gets an URG packet, start the open the receive file, send an ack and start the receive timer.
// If the receiver gets a DATA packet and the sequence number matches the expected sequence, write the packet to
// the receive file.
void Transport::receiverHandleData(DataPacket *data)
{
    if(data->packetType == URG)
    {
        qDebug() << "recieved urg";
        contendTimer->stop();

        if(!recvFile->isOpen())
        {
            emit(openDebug(sock->localAddress().toString(), sock->localPort()));
            QString name(data->data);
            recvFile->setFileName(name.insert(0,'1'));
            recvFile->open(QIODevice::WriteOnly | QIODevice::Text);
        }
        emit(packetRecv(-1, URG));
        sendAckPack(0);
        emit(packetSent(0, ACK));
        receiveTimer->start(TIMEOUT);
        return;
    }
    qDebug() << "received data: " <<data->seqNum;
    if((data->packetType & DATA) == DATA && data->seqNum == expectSeq)
    {
        receiveTimer->stop();
        recvFile->write(data->data, PAYLOADLEN);
        emit(packetRecv(data->seqNum, DATA));
        sendAckPack(++expectSeq);
        emit(packetSent(expectSeq, ACK));
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
    else
    {
        emit(packetRecv(data->seqNum, DATA));
    }
}

// Generates an ACK packet and sends it
void Transport::sendAckPack(int ackNum)
{
    qDebug() << "sending ack";
    ControlPacket ack;
    ack.packetType = ACK;
    ack.ackNum = ackNum;
    sock->writeDatagram(reinterpret_cast<char *>(&ack), sizeof(ControlPacket), *destAddress, destPort);
}
