#include <QtCore/QDebug>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <QtWidgets/QApplication>

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
                qDebug() << "Detected" << bridge->name();

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


                //bridge->createUser("hue-test",
                //                   "alexanders-mbp",
                //                   "alexandernassian");

                bridge->setUserName("alexandernassian");
                bridge->setDeviceName("alexanders-mbp");

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
