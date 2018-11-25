#ifndef TRANSPORTDEBUG_H
#define TRANSPORTDEBUG_H

#include <QWidget>
#include <QPushButton>

#include "packet.h"

namespace Ui {
class TransportDebug;
}

class TransportDebug : public QWidget
{
    Q_OBJECT

public:
    explicit TransportDebug(unsigned short windowSize, QWidget *parent = nullptr);
    ~TransportDebug();


public slots:
    void addSentPack(int,int);
    void addRecvPack(int,int);
    void retrans(int, int);
    void resetWindow();

private:
    Ui::TransportDebug *ui;
    unsigned short windowSize;
    QVector<QPushButton *> window;
    int head;
    QString styleDefault, styleSent, styleAcked, styleRetransmit;
    bool left;
};

#endif // TRANSPORTDEBUG_H
