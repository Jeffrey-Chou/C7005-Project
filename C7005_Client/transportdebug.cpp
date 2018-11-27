#include "transportdebug.h"
#include "ui_transportdebug.h"

void switchOnType(int type, QString& message)
{
    switch(type)
    {
    case URG:
        message.append("URG Packet");
        break;
    case DATA:
        message.append("DATA Packet ");
        break;
    case ACK:
        message.append("ACK Packet ");
        break;
    case DATA+FIN:
        message.append("FIN + DATA Packet ");
        break;
    default:
        break;
    }
}

TransportDebug::TransportDebug(QString ip, unsigned short port, unsigned short windowSize, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TransportDebug),
    windowSize(windowSize),
    logFile(new QFile())
{
    ui->setupUi(this);
    int width = ui->scrollArea->width()/windowSize ;
    int height = ui->scrollArea->height() - 2;
    styleDefault.append("QPushButton{ border: 1px solid #000000; }");
    styleSent.append("QPushButton{ border: 1px solid #000000; background-color: lightblue}");
    styleAcked.append("QPushButton{ border: 1px solid #000000; background-color: green}");
    styleRetransmit.append("QPushButton{ border: 1px solid #000000; background-color: red}");
    QWidget *widg = new QWidget();
    ui->scrollArea->setWidget(widg);
    widg->setFixedHeight(height );
    widg->setFixedWidth( width * windowSize );
    for(int i = 0; i < windowSize; ++i)
    {
        QPushButton * butt = new QPushButton(QString::number(i), widg);
        butt->setFixedWidth(width );
        butt->setFixedHeight(height);
        butt->move(i * width , 0);
        butt->setStyleSheet(styleDefault);
        window.append(butt);


    }
    left = true;
    head = 0;
    logFile->setFileName(ip + ":" + QString::number(port) + ":" + QDate::currentDate().toString("yyyy.MM.dd") +".log");
    logFile->open(QIODevice::Append | QIODevice::Text);
    logStream.setDevice(logFile);
    QString currTime(QTime::currentTime().toString("hh:mm:ss:zzz"));
    ui->textEdit->append("Start time: " + currTime);
    logStream << "Start time: " << currTime << "\n";



}

TransportDebug::~TransportDebug()
{
    delete ui;
}

void TransportDebug::addSentPack(int index, int type)
{
    if(!left)
    {
        ui->textEdit->insertPlainText("\n");
        ui->textEdit->setAlignment(Qt::AlignLeft);
        left = true;
    }

    QString message("Sent: ");
    switchOnType(type, message);
    if(index >= 0)
    {
        if(type == DATA || type == DATA + FIN)
            window[index]->setStyleSheet(styleSent);
        message.append(QString::number(index));
    }
    message.append(" ");
    message.append(QTime::currentTime().toString("hh:mm:ss:zzz"));
    ui->textEdit->append(message);
    logStream << message << "\n";


}

void TransportDebug::addRecvPack(int index, int type)
{

    if(left)
    {
        ui->textEdit->insertPlainText("\n");
        ui->textEdit->setAlignment(Qt::AlignRight);
        left = false;
    }
    QString message("Received: ");
    switchOnType(type, message);
    if(index >= 0)
    {
        while(head < index && type == ACK)
            window[head++]->setStyleSheet(styleAcked);
        message.append(QString::number(index));
    }
    message.append(" ");
    message.append(QTime::currentTime().toString("hh:mm:ss:zzz"));
    ui->textEdit->append(message);
    logStream << message << "\n";


}

void TransportDebug::resetWindow()
{
    head = 0;
    for(int i = 0; i < windowSize; ++i)
    {
        window[i]->setStyleSheet(styleDefault);
    }
}

void TransportDebug::retrans(int start, int end, int type)
{
    if(!left)
    {
        ui->textEdit->insertPlainText("\n");
        ui->textEdit->setAlignment(Qt::AlignLeft);
        left = true;
    }

    for(int i = start; i < end; ++i)
    {
        window[i]->setStyleSheet(styleRetransmit);
    }
    QString message("Retransmitting ");
    if(type == DATA)
    {
        message.append("Data Packets From: ");
        message.append(QString::number(start));
        message.append(" to ");
        message.append(QString::number(end));
    }
    if(type == ACK)
    {
        message.append("ACK Packet " + QString::number(end));
    }
    message.append(" ");
    message.append(QTime::currentTime().toString("hh:mm:ss:zzz"));
    ui->textEdit->append(message);
    logStream << message << "\n";
}

void TransportDebug::closeDebug()
{
    if(!left)
    {
        ui->textEdit->insertPlainText("\n");
        ui->textEdit->setAlignment(Qt::AlignLeft);
        left = true;
    }
    QString currTime(QTime::currentTime().toString("hh:mm:ss:zzz"));
    ui->textEdit->append("End time: " + currTime);
    logStream << "End time: " << currTime << "\n\n";
    logStream.flush();
    logFile->close();

}
