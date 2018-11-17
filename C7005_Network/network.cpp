#include "network.h"
#include "ui_network.h"

#include <QDebug>

Network::Network(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Network),
    fromMac01(new QUdpSocket(this)),
    fromMac02(new QUdpSocket(this)),
    errorRate(0),
    isListen(false)
{
    ui->setupUi(this);
    loadConfig();
    ui->lineEdit_error->setText("0");
    memset(mac01Buffer, 0, BUFFSIZE);
    memset(mac02Buffer, 0, BUFFSIZE);
    connect(ui->horizontalSlider_BER, SIGNAL(valueChanged(int)), this, SLOT(handleSliderChange(int)));
    connect(ui->pushButton, SIGNAL(pressed()), this, SLOT(listen()));

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
        QHostAddress networkAddr(ui->lineEdit_net_ip->text());
        mac01IP.setAddress(ui->lineEdit_mac01_ip->text());
        mac01Port = ui->lineEdit_mac01_port->text().toUShort();

        mac02IP.setAddress(ui->lineEdit_mac02_ip->text());
        mac02Port = ui->lineEdit_mac02_port->text().toUShort();

        srand(time(NULL));

        fromMac01->bind(networkAddr, ui->lineEdit_net_port_1->text().toUShort());
        fromMac02->bind(networkAddr, ui->lineEdit_net_port_2->text().toUShort());

        connect(fromMac01, SIGNAL(readyRead()), this, SLOT(sendToMac02()));
        connect(fromMac02, SIGNAL(readyRead()), this, SLOT(sendToMac01()));

    }
    else
    {
        ui->pushButton->setText("Start");
        fromMac01->close();
        fromMac02->close();
    }
}

void Network::sendToMac01()
{
    while(fromMac02->hasPendingDatagrams())
    {
        qint64 bytesRead = fromMac02->readDatagram(mac02Buffer, BUFFSIZE);
        if(forwardPacket())
        {
            fromMac02->writeDatagram(mac02Buffer,bytesRead, mac01IP, mac01Port);
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
            fromMac01->writeDatagram(mac01Buffer,bytesRead, mac02IP, mac02Port);
        }
    }
}
