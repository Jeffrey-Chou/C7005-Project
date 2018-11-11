#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <QObject>
#include <QUdpSocket>
#include <QFile>

class Transport : public QObject
{
    Q_OBJECT
public:
    //explicit Transport(QObject *parent = nullptr);
    explicit Transport(QString ip, unsigned short port, QString destIP, unsigned short destPort, QObject *parent = nullptr);
    ~Transport();




signals:

public slots:
    void send(QFile *file);
    void receive();

private:
    QUdpSocket *sock;
    QHostAddress *destAddress;
    unsigned short destPort;
};

#endif // TRANSPORT_H
