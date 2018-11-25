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

TransportDebug::TransportDebug(unsigned short windowSize, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TransportDebug),
    windowSize(windowSize)
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
        window[index]->setStyleSheet(styleSent);
        message.append(QString::number(index));
    }

    ui->textEdit->append(message);

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
        while(head < index)
            window[head++]->setStyleSheet(styleAcked);
        message.append(QString::number(index));
    }

    ui->textEdit->append(message);


}

void TransportDebug::resetWindow()
{
    head = 0;
    for(int i = 0; i < windowSize; ++i)
    {
        window[i]->setStyleSheet(styleDefault);
    }
}

void TransportDebug::retrans(int start, int end)
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
    QString message("Retransmitting from: ");
    message.append(QString::number(start));
    message.append(" to ");
    message.append(QString::number(end));
    ui->textEdit->append(message);
}
