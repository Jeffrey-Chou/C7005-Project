#include "client.h"
#include "ui_client.h"

#include <QDebug>
#include <QFileDialog>

Client::Client(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Client),
    sendFile(new QFile())
{
    ui->setupUi(this);
    loadConfig();
    connect(ui->pushButton_file, SIGNAL(clicked()), this, SLOT(loadFile()));
    connect(ui->pushButton_send, SIGNAL(clicked()), this, SLOT(send()));
}

Client::~Client()
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

void Client::loadFile()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Select a file"));
    qDebug() << filename;
    if(filename.size() == 0)
    {
        return;
    }
    sendFile->setFileName(filename);
    ui->lineEdit_file->setText(filename);
}

void Client::send()
{
    int window;
    QString hostIP, destIP;
    short hostPort, destPort;
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
        hostPort = ui->lineEdit_mac01_port->text().toShort();
        destPort = ui->lineEdit_net_port_1->text().toShort();
    }
    else
    {
        hostIP = ui->lineEdit_mac02_ip->text();
        hostPort = ui->lineEdit_mac02_port->text().toShort();
        destPort = ui->lineEdit_net_port_2->text().toShort();
    }

    qDebug() << "host ip and port:" << hostIP << ":" << hostPort;
    qDebug() << "dest ip and port: " << destIP << ":" << destPort;
    qDebug() << "window size: " << window;
    qDebug() << "file to send: " << sendFile->fileName();


}
