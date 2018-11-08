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
