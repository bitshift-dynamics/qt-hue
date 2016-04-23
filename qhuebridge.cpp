#include "qhuebridge.h"

#include <QtCore/QDebug>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QMap>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>


class QHueBridgePrivate : public QObject
{
        Q_OBJECT
        Q_DECLARE_PUBLIC(QHueBridge)
        Q_DISABLE_COPY(QHueBridgePrivate)

public:
        enum HueRequestType {
                UnknownRequest,
                CreateUserRequest,
                GetConfigurationRequest
        };

        QNetworkAccessManager* qnam;
        QMap<QNetworkReply*, QHueBridgePrivate::HueRequestType> requestQueue;

        QString id; //!< \todo Check if replaced by configuration obj.
        QString ipAddress; //!< \todo Check if replaced by configuration obj.
        QString hardwareAddress; //!< \todo Check if replaced by configuration obj.
        QString name; //!< \todo Check if replaced by configuration obj.
        QHueBridge::Configuration configuration;

        QString userName;
        QString deviceName;

        explicit QHueBridgePrivate(QHueBridge* parent);

public slots:
        void handleSslErrors(QNetworkReply* reply,
                             const QList<QSslError>& errors);
        void handleReply(QNetworkReply* reply);

private:
        QHueBridge* q_ptr;
};

#include "qhuebridge.moc"


QHueBridgePrivate::QHueBridgePrivate(QHueBridge* parent) :
        QObject(parent),
        q_ptr(parent)
{
        bool isConnected = false;
        Q_UNUSED(isConnected);


        qnam = new(std::nothrow) QNetworkAccessManager(this);
        Q_CHECK_PTR(qnam);

        isConnected = QObject::connect(
                              qnam,
                              SIGNAL(sslErrors(QNetworkReply*,
                                               QList<QSslError>)),
                              this,
                              SLOT(handleSslErrors(QNetworkReply*,
                                                   QList<QSslError>)));
        Q_ASSERT(isConnected == true);

        isConnected = QObject::connect(qnam,
                                       SIGNAL(finished(QNetworkReply*)),
                                       this,
                                       SLOT(handleReply(QNetworkReply*)));
        Q_ASSERT(isConnected == true);
}

void QHueBridgePrivate::handleSslErrors(QNetworkReply* reply,
                                        const QList<QSslError>& errors)
{
        qWarning() << "Ignoring SSL errors.";
        reply->ignoreSslErrors(errors);
}

void QHueBridgePrivate::handleReply(QNetworkReply* reply)
{
        Q_Q(QHueBridge);


        if (requestQueue.contains(reply) == false) {
                qCritical() << "Got a reply without a request.";
                return;
        }

        QHueBridgePrivate::HueRequestType requestType = requestQueue[reply];

        QByteArray replyData = reply->readAll();
        QJsonParseError jsonError;
        QJsonDocument replyDocument = QJsonDocument::fromJson(replyData,
                                                              &jsonError);

        // Check the received reply data.
        if (jsonError.error != QJsonParseError::NoError) {
                qCritical() << "Received invalid JSON document" << replyData;
                goto fail;
        }

        // Pass the reply data to the specific handler.
        if (requestType == QHueBridgePrivate::CreateUserRequest) {
                // According to the documentation the result is always
                // a list containing a single item that indicates success
                // or an error.
                QJsonObject result = replyDocument.array().at(0).toObject();
                if (result.contains("error") == true) {
                        QJsonObject errorObject = result.value("error")
                                                  .toObject();
                        QHueError error;
                        error.error = static_cast<QHueError::ErrorType>(
                                              errorObject.value("type")
                                              .toInt());
                        error.address = errorObject.value("address")
                                        .toString();
                        error.text = errorObject.value("description")
                                     .toString();

                        emit q->error(error);
                        goto fail;
                }
                else if (result.contains("success") == true) {
                        emit q->userCreated(result
                                            .value("success")
                                            .toObject()
                                            .value("username")
                                            .toString());
                        goto success;
                }
                else {
                        qWarning() << "Response contains unknown key."
                                    << replyData;
                        goto fail;
                }
        }
        else if (requestType == QHueBridgePrivate::GetConfigurationRequest) {
                QJsonObject configurationObject = replyDocument.object();

                // Reset configuration
                //! \todo
                configuration = QHueBridge::Configuration();

                configuration.name = configurationObject.value("name").toString();
                configuration.bridgeDateTime = QDateTime::fromString(configurationObject.value("UTC").toString());
                configuration.localDateTime = QDateTime::fromString(configurationObject.value("localtime").toString());
                configuration.timezone = configurationObject.value("timezone").toString();
                configuration.hardwareAddress = configurationObject.value("mac").toString();
                configuration.ipAddress = configurationObject.value("ipaddress").toString();
                configuration.gatewayIpAddress = configurationObject.value("gateway").toString();
                configuration.subnetIpAddress = configurationObject.value("netmask").toString();
                configuration.proxyAddress = configurationObject.value("proxyaddress").toString();
                configuration.proxyPort = static_cast<quint16>(configurationObject.value("proxyport").toInt());
                configuration.linkButtonPressed = configurationObject.value("linkbutton").toBool();
                configuration.apiVersion = configurationObject.value("apiversion").toString();
                configuration.version = configurationObject.value("swversion").toString();
                configuration.zigbeeChannel = static_cast<quint8>(configurationObject.value("zigbeechannel").toInt());
                configuration.portalConnection = configurationObject.value("portalconnection").toString();
                configuration.portalAccountLinked = configurationObject.value("portalservices").toBool();

                QJsonObject portalStateObject = configurationObject.value("portalstate").toObject();
                configuration.portalState.communicationState = portalStateObject.value("communication").toString();
                configuration.portalState.incomingEnabled = portalStateObject.value("incoming").toBool();
                configuration.portalState.outgoingEnabled = portalStateObject.value("outgoing").toBool();
                configuration.portalState.isSignedOn = portalStateObject.value("signedon").toBool();

                QJsonObject whitelistObject = configurationObject.value("whitelist").toObject();
                QStringList whitelistIds = whitelistObject.keys();
                foreach (QString id, whitelistIds) {
                        QJsonObject obj = whitelistObject.value(id).toObject();

                        QHueBridge::WhitelistEntry entry;
                        entry.id = id;
                        entry.createDateTime = QDateTime::fromString(obj.value("create date").toString());
                        entry.lastUseDateTime = QDateTime::fromString(obj.value("last use date").toString());
                        entry.name = obj.value("name").toString();

                        configuration.whitelist.append(entry);
                }

                emit q->configurationUpdated(configuration);
        }
        else {
              qCritical() << "Failed handling response of"
                          << reply
                          << requestType;
              goto fail;
        }


fail:
success:
        requestQueue.remove(reply);
}


QHueBridge::QHueBridge(QObject* parent) :
        QObject(parent),
        d_ptr(new QHueBridgePrivate(this))
{
}

void QHueBridge::setId(const QString& id)
{
        Q_D(QHueBridge);
        d->id = id;
}

QString QHueBridge::id() const
{
        Q_D(const QHueBridge);
        return d->id;
}

void QHueBridge::setIpAddress(const QString& ipAddress)
{
        Q_D(QHueBridge);
        d->ipAddress = ipAddress;
}

QString QHueBridge::ipAddress() const
{
        Q_D(const QHueBridge);
        return d->ipAddress;
}

void QHueBridge::setHardwareAddress(const QString& hardwareAddress)
{
        Q_D(QHueBridge);
        d->hardwareAddress = hardwareAddress;
}

QString QHueBridge::hardwareAddress() const
{
        Q_D(const QHueBridge);
        return d->hardwareAddress;
}

void QHueBridge::setName(const QString& name)
{
        Q_D(QHueBridge);
        d->name = name;
}

QString QHueBridge::name() const
{
        Q_D(const QHueBridge);
        return d->name;
}

void QHueBridge::setUserName(const QString& user)
{
        Q_D(QHueBridge);
        d->userName = user;
}

QString QHueBridge::userName() const
{
        Q_D(const QHueBridge);
        return d->userName;
}

void QHueBridge::setDeviceName(const QString& device)
{
        Q_D(QHueBridge);
        d->deviceName = device;
}

QString QHueBridge::deviceName() const
{
        Q_D(const QHueBridge);
        return d->deviceName;
}

void QHueBridge::createUser(const QString& applicationName,
                            const QString& deviceName,
                            const QString& userName)
{
        Q_D(QHueBridge);


        QVariantMap arguments;
        arguments["devicetype"] = QString("%1#%2")
                                  .arg(applicationName)
                                  .arg(deviceName);
        if (userName.isEmpty() == false)
                arguments["username"] = userName;


        // Some data checking agains the API reference.
        if (arguments["devicetype"].toString().length() > 40) {
                qCritical() << "Devicetype too long, max. 40 characters.";
                return;
        }

        if (userName.length() < 10 || userName.length() > 40) {
                qCritical() << "Username length invalid, must be between 10 "
                               "and 40 characters.";
                return;
        }


        QNetworkRequest request(QUrl(QString("http://%1/api")
                                     .arg(ipAddress())));
        QByteArray requestData = QJsonDocument::fromVariant(arguments)
                                 .toJson();
        qDebug() << "Sending CreateUser request" << requestData;

        QNetworkReply* reply = d->qnam->post(request, requestData);
        d->requestQueue[reply] = QHueBridgePrivate::CreateUserRequest;
}

void QHueBridge::requestConfiguration()
{
        Q_D(QHueBridge);

        QNetworkRequest request(QUrl(QString("http://%1/api/%2/config")
                                     .arg(ipAddress())
                                     .arg(userName())));
        qDebug() << "Sending configuration request to" << request.url();

        QNetworkReply* reply = d->qnam->get(request);
        d->requestQueue[reply] = QHueBridgePrivate::GetConfigurationRequest;
}
