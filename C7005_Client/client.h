#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QFile>

namespace Ui {
class Client;
}

class Client : public QMainWindow
{
    Q_OBJECT

public:
    explicit Client(QWidget *parent = nullptr);
    ~Client();

public slots:
    void loadFile();

private:
    Ui::Client *ui;
    QFile *sendFile;
    void loadConfig();
};

#endif // CLIENT_H
