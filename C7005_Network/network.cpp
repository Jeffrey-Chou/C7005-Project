#include "network.h"
#include "ui_network.h"

#include <QDebug>
#include <QTime>

Network::Network(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Network),
    fromMac01(new QUdpSocket(this)),
    fromMac02(new QUdpSocket(this)),
    errorRate(0),
    delay(0),
    isListen(false)
{
    ui->setupUi(this);
    loadConfig();
    ui->lineEdit_error->setText("0");
    ui->lineEdit_Delay->setText("0");
    memset(mac01Buffer, 0, BUFFSIZE);
    memset(mac02Buffer, 0, BUFFSIZE);
    connect(ui->horizontalSlider_BER, SIGNAL(valueChanged(int)), this, SLOT(handleSliderChange(int)));
    connect(ui->pushButton, SIGNAL(pressed()), this, SLOT(listen()));
    toMac01Timer.setSingleShot(true);
    toMac02Timer.setSingleShot(true);

}

Network::~Network()
{
    delete ui;
}

bool Network::forwardPacket()
{
    return (rand() % 100 + 1) > errorRate;
}

void parseData(QList<QByteArray> &lineSub, QWidget *widg)
{
    QList<QLineEdit *> lineEdits = widg->findChildren<QLineEdit *>();
    for(int i = 0; i < lineEdits.size(); ++i)
    {
        lineEdits[i]->insert(lineSub.at(i + 1));
    }
}

void Network::loadConfig()
{
    QFile conf("common.conf");
    if(!conf.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    while(!conf.atEnd())
    {
        QByteArray line = conf.readLine();
        QList<QByteArray> lineSub = line.split(':');
        if(line.startsWith("machine1"))
        {
            parseData(lineSub, ui->machine1Widget);
        }
        else if(line.startsWith("machine2"))
        {
            parseData(lineSub, ui->machine2Widget);
        }
        else
        {
            parseData(lineSub, ui->networkWidget);
        }
    }
    conf.close();
}

void Network::handleSliderChange(int value)
{
    ui->lineEdit_error->setText(QString::number(value));
    errorRate = value;
}

void Network::listen()
{
    isListen = !isListen;
    if(isListen)
    {
        ui->pushButton->setText("Stop");
        delay = ui->lineEdit_Delay->text().toInt();        QHostAddress networkAddr(ui->lineEdit_net_ip->text());
        mac01IP.setAddress(ui->lineEdit_mac01_ip->text());
        mac01Port = ui->lineEdit_mac01_port->text().toUShort();

        mac02IP.setAddress(ui->lineEdit_mac02_ip->text());
        mac02Port = ui->lineEdit_mac02_port->text().toUShort();

        srand(time(NULL));

        fromMac01->bind(networkAddr, ui->lineEdit_net_port_1->text().toUShort());
        fromMac02->bind(networkAddr, ui->lineEdit_net_port_2->text().toUShort());

        connect(fromMac01, SIGNAL(readyRead()), this, SLOT(sendToMac02()));
        connect(fromMac02, SIGNAL(readyRead()), this, SLOT(sendToMac01()));
        connect(&toMac01Timer, SIGNAL(timeout()), this, SLOT(toMac01DelayOver()));
        connect(&toMac02Timer, SIGNAL(timeout()), this, SLOT(toMac02DelayOver()));

    }
    else
    {
        ui->pushButton->setText("Start");
        fromMac01->close();
        fromMac02->close();
        toMac01Timer.stop();
        toMac02Timer.stop();
        disconnect(fromMac01, SIGNAL(readyRead()), this, SLOT(sendToMac02()));
        disconnect(fromMac02, SIGNAL(readyRead()), this, SLOT(sendToMac01()));
        disconnect(&toMac01Timer, SIGNAL(timeout()), this, SLOT(toMac01DelayOver()));
        disconnect(&toMac02Timer, SIGNAL(timeout()), this, SLOT(toMac02DelayOver()));

        toMac01Queue.clear();
        toMac01PacketSize.clear();

        toMac02Queue.clear();
        toMac02PacketSize.clear();
    }
}

void Network::sendToMac01()
{
    while(fromMac02->hasPendingDatagrams())
    {
        qint64 bytesRead = fromMac02->readDatagram(mac02Buffer, BUFFSIZE);
        if(forwardPacket())
        {
            //QTime time = QTime::currentTime();
            //while(time.msecsTo(QTime::currentTime()) < delay);
            //fromMac02->writeDatagram(mac02Buffer,bytesRead, mac01IP, mac01Port);
            char * packet = new char[bytesRead];
            memcpy(packet, mac02Buffer, bytesRead);
            toMac01Queue.enqueue(packet);
            toMac01PacketSize.enqueue(bytesRead);
            if(!toMac01Timer.isActive())
            {
                toMac01Timer.start(delay);
            }

        }
    }
}

void Network::sendToMac02()
{
    while(fromMac01->hasPendingDatagrams())
    {
        qint64 bytesRead = fromMac01->readDatagram(mac01Buffer, BUFFSIZE);
        if(forwardPacket())
        {
            //QTime time = QTime::currentTime();
            //while(time.msecsTo(QTime::currentTime()) < delay);
            //fromMac01->writeDatagram(mac01Buffer,bytesRead, mac02IP, mac02Port);
            char * packet = new char[bytesRead];
            memcpy(packet, mac01Buffer, bytesRead);
            toMac02Queue.enqueue(packet);
            toMac02PacketSize.enqueue(bytesRead);
            if(!toMac02Timer.isActive())
            {
                toMac02Timer.start(delay);
            }
        }
    }
}

void Network::toMac01DelayOver()
{
    char *packet = toMac01Queue.dequeue();
    long long bytesToSend = toMac01PacketSize.dequeue();

    if((packet[3] & URG) == URG || (packet[3] & DATA) == DATA)
    {
        bytesToSend += PAYLOADLEN;
    }
    fromMac02->writeDatagram(packet,bytesToSend, mac01IP, mac01Port);
    qDebug() << "sending to machine 1";
    delete packet;
    if(!toMac01Queue.isEmpty())
    {
        toMac01Timer.start(delay);
    }

}

void Network::toMac02DelayOver()
{
    char *packet = toMac02Queue.dequeue();
    long long bytesToSend = toMac02PacketSize.dequeue();

    //qDebug() << (packet[3] & 0x08);
    //if((packet[3] & URG) == 0x08 || (packet[3] & DATA) == 0x02)
    //{
    //    bytesToSend += PAYLOADLEN;
    //}
    fromMac01->writeDatagram(packet,bytesToSend, mac02IP, mac02Port);
    qDebug() << "sending to machine 2 this many bytes:" << bytesToSend;
    delete packet;
    if(!toMac02Queue.isEmpty())
    {
        toMac02Timer.start(delay);
    }

}
