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
    connect(ui->horizontalSlider_BER, SIGNAL(valueChanged(int)), this, SLOT(handleSliderChange(int)));
    connect(ui->pushButton, SIGNAL(pressed()), this, SLOT(listen()));
}

Network::~Network()
{
    delete ui;
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

    }
    else
    {
        ui->pushButton->setText("Start");
    }
}
