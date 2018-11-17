#ifndef NETWORK_H
#define NETWORK_H

#include <QMainWindow>
#include <QFile>
#include <QUdpSocket>

namespace Ui {
class Network;
}

class Network : public QMainWindow
{
    Q_OBJECT

public:
    explicit Network(QWidget *parent = nullptr);
    ~Network();

public slots:
    void handleSliderChange(int);
    void listen();

private:
    Ui::Network *ui;
    QUdpSocket *fromMac01, *fromMac02;
    QHostAddress mac01IP, mac02IP;
    unsigned short mac01Port, mac02Port;

    int errorRate;

    bool isListen;


    void loadConfig();
};

#endif // NETWORK_H
