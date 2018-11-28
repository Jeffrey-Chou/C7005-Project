#include "client.h"
#include "ui_client.h"

#include <QDebug>
#include <QFileDialog>


Client::Client(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Client),
    sendFile(new QFile()),
    transport(nullptr),
    debug(nullptr)
{
    ui->setupUi(this);
    loadConfig();
    ui->lineEdit_window->setText("8");


    connect(ui->pushButton_file, SIGNAL(clicked()), this, SLOT(loadFile()));
    connect(ui->pushButton_send, SIGNAL(clicked()), this, SLOT(send()));
    connect(ui->radioButton_mac1, SIGNAL(clicked()), this, SLOT(configureTransport()));
    connect(ui->radioButton_mac2, SIGNAL(clicked()), this, SLOT(configureTransport()));
}

Client::~Client()
{
    delete ui;
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
void Client::loadConfig()
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

// Opens a file dialog the load a file to send
void Client::loadFile()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Select a file"));
    qDebug() << filename;
    if(filename.size() == 0)
    {
        return;
    }
    if(sendFile->isOpen())
    {
        sendFile->close();
    }
    sendFile->setFileName(filename);
    sendFile->open(QIODevice::ReadOnly | QIODevice::Text);
    transport->setFile(sendFile);
    ui->lineEdit_file->setText(filename);
}

// Called when the send button is pressed. Takes the information from the text lines to send data
void Client::send()
{
    int window;
    QString hostIP, destIP;
    unsigned short hostPort, destPort;
    if(ui->lineEdit_window->text() == "")
    {
        qDebug() << "window not set";
        return;
    }
    window = ui->lineEdit_window->text().toInt();

    if(sendFile->fileName() == "")
    {
        qDebug() << "file not set";
        return;

    }
    destIP = ui->lineEdit_net_ip_1->text();
    if(ui->radioButton_mac1->isChecked())
    {
        hostIP = ui->lineEdit_mac01_ip->text();
        hostPort = ui->lineEdit_mac01_port->text().toUShort();
        destPort = ui->lineEdit_net_port_1->text().toUShort();
    }
    else
    {
        hostIP = ui->lineEdit_mac02_ip->text();
        hostPort = ui->lineEdit_mac02_port->text().toUShort();
        destPort = ui->lineEdit_net_port_2->text().toUShort();
    }

    openDebugWindow(hostIP, hostPort);

    qDebug() << "host ip and port:" << hostIP << ":" << hostPort;
    qDebug() << "dest ip and port: " << destIP << ":" << destPort;
    qDebug() << "window size: " << window;
    qDebug() << "file to send: " << sendFile->fileName();
    emit(readyToSend(true));


}

// Configures the transport class from the text lines
void Client::configureTransport()
{
    if(transport)
        delete transport;

    if(ui->radioButton_mac1->isChecked())
    {
        transport = new Transport(ui->lineEdit_mac01_ip->text(), ui->lineEdit_mac01_port->text().toUShort(),
                                  ui->lineEdit_net_ip_1->text(), ui->lineEdit_net_port_1->text().toUShort(),
                                  ui->lineEdit_window->text().toUShort(), this);
    }
    if(ui->radioButton_mac2->isChecked())
    {
        transport = new Transport(ui->lineEdit_mac02_ip->text(), ui->lineEdit_mac02_port->text().toUShort(),
                                  ui->lineEdit_net_ip_1->text(), ui->lineEdit_net_port_2->text().toUShort(),
                                  ui->lineEdit_window->text().toUShort(), this);
    }
    connect(transport, SIGNAL(openDebug(QString, unsigned short)), this, SLOT(openDebugWindow(QString, unsigned short)));

}

// Opens the debug menu when signalled.
void Client::openDebugWindow(QString ip, unsigned short port)
{
    if(debug == nullptr)
    {
        debug = new TransportDebug(ip, port, ui->lineEdit_window->text().toUShort());
        debug->show();
        connect(transport, SIGNAL(packetSent(int,int)), debug, SLOT(addSentPack(int,int)));
        connect(transport, SIGNAL(packetRecv(int,int)), debug, SLOT(addRecvPack(int,int)));
        connect(transport, SIGNAL(beginReset()), debug, SLOT(resetWindow()));
        connect(transport, SIGNAL(retransmit(int,int,int)), debug, SLOT(retrans(int,int,int)));
        connect(transport, SIGNAL(finished()), debug, SLOT(closeDebug()));
    }
}
