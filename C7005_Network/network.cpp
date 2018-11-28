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

// Determines whether to forward a packet or not
bool Network::forwardPacket()
{
    return (rand() % 100 + 1) > errorRate;
}

// Parses the data in the config file and writes it to the text line
void parseData(QList<QByteArray> &lineSub, QWidget *widg)
{
    QList<QLineEdit *> lineEdits = widg->findChildren<QLineEdit *>();
    for(int i = 0; i < lineEdits.size(); ++i)
    {
        lineEdits[i]->insert(lineSub.at(i + 1));
    }
}
// Opens the config file
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

// Updates error text line when slider changes
void Network::handleSliderChange(int value)
{
    ui->lineEdit_error->setText(QString::number(value));
    errorRate = value;
}

// Starts and stops the network from listening when pressed
void Network::listen()
{
    isListen = !isListen;
    if(isListen)
    {
        toMac01Forwarded = toMac01Dropped = toMac02Forwarded = toMac02Dropped = 0;
        ui->lineEdit_toMac01F->setText(QString::number(0));
        ui->lineEdit_toMac01D->setText(QString::number(0));
        ui->lineEdit_toMac02F->setText(QString::number(0));
        ui->lineEdit_toMac02D->setText("0");
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

// Receives packets from machine 2 to send to machine 1.
// If the packet will be forwarded, add it to the queue and start the delay timer
void Network::sendToMac01()
{
    while(fromMac02->hasPendingDatagrams())
    {
        qint64 bytesRead = fromMac02->readDatagram(mac02Buffer, BUFFSIZE);
        if(forwardPacket())
        {

            char * packet = new char[bytesRead];
            memcpy(packet, mac02Buffer, static_cast<size_t>(bytesRead));
            toMac01Queue.enqueue(packet);
            toMac01PacketSize.enqueue(bytesRead);
            if(!toMac01Timer.isActive())
            {
                toMac01Timer.start(delay);
            }

        }
        else
        {
            ui->lineEdit_toMac01D->setText(QString::number(++toMac01Dropped));
        }
    }
}

// Receives packets from machine 1 to send to machine 2.
// If the packet will be forwarded, add it to the queue and start the delay timer
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
            memcpy(packet, mac01Buffer, static_cast<size_t>(bytesRead));
            toMac02Queue.enqueue(packet);
            toMac02PacketSize.enqueue(bytesRead);
            if(!toMac02Timer.isActive())
            {
                toMac02Timer.start(delay);
            }
        }
        else
        {
            ui->lineEdit_toMac02D->setText(QString::number(++toMac02Dropped));
        }
    }
}

// Called when the to machine 1 timer has elapsed. Grabs the first packet on the queue and sends it.
// If the queue has more packets restarts the timer.
void Network::toMac01DelayOver()
{
    char *packet = toMac01Queue.dequeue();
    long long bytesToSend = toMac01PacketSize.dequeue();
    fromMac02->writeDatagram(packet,bytesToSend, mac01IP, mac01Port);
    qDebug() << "sending to machine 1";
    delete packet;
    ui->lineEdit_toMac01F->setText(QString::number(++toMac01Forwarded));
    if(!toMac01Queue.isEmpty())
    {
        toMac01Timer.start(delay);
    }

}

// Called when the to machine 2 timer has elapsed. Grabs the first packet on the queue and sends it.
// If the queue has more packets restarts the timer.
void Network::toMac02DelayOver()
{
    char *packet = toMac02Queue.dequeue();
    long long bytesToSend = toMac02PacketSize.dequeue();
    fromMac01->writeDatagram(packet,bytesToSend, mac02IP, mac02Port);
    ui->lineEdit_toMac02F->setText(QString::number(++toMac02Forwarded));
    qDebug() << "sending to machine 2";
    delete packet;
    if(!toMac02Queue.isEmpty())
    {
        toMac02Timer.start(delay);
    }

}
