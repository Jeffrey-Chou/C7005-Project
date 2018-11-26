#ifndef TRANSPORTDEBUG_H
#define TRANSPORTDEBUG_H

#include <QWidget>
#include <QPushButton>
#include <QFile>
#include <QTime>
#include <QDate>
#include <QTextStream>

#include "packet.h"

namespace Ui {
class TransportDebug;
}

class TransportDebug : public QWidget
{
    Q_OBJECT

public:
    explicit TransportDebug(QString ip, unsigned short port, unsigned short windowSize, QWidget *parent = nullptr);
    ~TransportDebug();


public slots:
    void addSentPack(int,int);
    void addRecvPack(int,int);
    void retrans(int, int,int);
    void resetWindow();
    void closeDebug();

private:
    Ui::TransportDebug *ui;
    unsigned short windowSize;
    QVector<QPushButton *> window;
    int head;
    QString styleDefault, styleSent, styleAcked, styleRetransmit;
    bool left;
    QFile *logFile;
    QTextStream logStream;

};

#endif // TRANSPORTDEBUG_H
