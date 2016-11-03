#include <QtCore/QDebug>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <QtWidgets/QApplication>

#include <QtConcurrent/QtConcurrentRun>

#include "qhuebridgemanager.h"


int main(int argc, char** argv)
{
        QApplication a(argc, argv);

        bool isConnected = false;
        Q_UNUSED(isConnected);


        QHueBridgeManager manager;

        isConnected = QObject::connect(&manager,
                                       &QHueBridgeManager::detected,
                                       [](QHueBridge* bridge) {
                qDebug() << "Detected" << bridge->id();

                bool isConnected = false;
                Q_UNUSED(isConnected);

                isConnected = QObject::connect(bridge,
                                               &QHueBridge::error,
                                               [](const QHueError& error) {
                        qCritical() << "Error" << error.error << error.text;
                });
                Q_ASSERT(isConnected == true);

                isConnected = QObject::connect(bridge,
                                               &QHueBridge::userCreated,
                                               [](const QString& userName) {
                        qDebug() << "Created user" << userName;
                });
                Q_ASSERT(isConnected == true);

                isConnected = QObject::connect(bridge,
                                               &QHueBridge::configurationUpdated,
                                               [](const QHueBridge::Configuration& config) {
                        qDebug() << config.version;
                });


                bridge->updateLights();

                QtConcurrent::run([=]() {
                    QDateTime start = QDateTime::currentDateTime();
                    int counter = 0;
                    QVector<int> lamps;
                    lamps << 1 << 4 << 5 << 6 << 7 << 8;
                    bool isFlipped = true;
                    while (true) {
                        if (start.msecsTo(QDateTime::currentDateTime()) > 100) {
                            if (counter == 0)
                                isFlipped = !isFlipped;
                            QMetaObject::invokeMethod(bridge,
                                                      "setLight",
                                                      Qt::QueuedConnection,
                                                      Q_ARG(int, lamps[counter]),
                                                      Q_ARG(quint16, isFlipped ? 50000 : 10000),
                                                      Q_ARG(quint8, 200),
                                                      Q_ARG(quint8, 50));
                            //bridge->setLight(qrand() % 6, qrand() % 50000, 200, 200, 4);
                            start = QDateTime::currentDateTime();

                            counter++;
                            if (counter == 6)
                                counter = 0;
                        }
                        //QThread::msleep(1000);
                    }
                });

                     /* QTimer* t = new QTimer;
                      t->setInterval(100);
                      QObject::connect(t, SIGNAL(timeout()), bridge, SLOT(requestConfiguration()));
                      t->start();*/

                      bridge->requestConfiguration();
        });
        Q_ASSERT(isConnected == true);

        manager.detect();

        return a.exec();
}
