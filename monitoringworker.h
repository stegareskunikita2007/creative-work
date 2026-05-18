#ifndef MONITORINGWORKER_H
#define MONITORINGWORKER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QTimer>
#include <QRandomGenerator>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include "stepikapiclient.h"
class SpellChecker;
#include "textprocessor.h"
#include "coursesearchengine.h"


class MonitoringWorker : public QObject
{
    Q_OBJECT

public:
    explicit MonitoringWorker(QObject *parent = nullptr);

    void setToken(const QString &token);
    void setGroupId(const QString &id);
    QString analyzeMessage(const QString &message);

signals:
    void logMessage(const QString &message, const QString &type);

public slots:
    void start();
    void stop();

private slots:
    void onStepikCoursesReady(const QVector<StepikCourse> &courses);

private:

    // Сеть
    QNetworkAccessManager *networkManager;
    QString vkToken;
    QString groupId;

    // Long Poll
    QString longPollServer;
    QString longPollKey;
    QString longPollTs;
    bool isRunning = false;

    // Long Poll методы
    void getLongPollServer();
    void startLongPoll();
    void processLongPollResponse(const QJsonObject &data);

    // Отправка сообщения
    void sendMessage(int peerId, const QString &message);

    QSet<QString> processedEventIds;

    StepikApiClient *stepikClient;
    int pendingPeerId;
    QString pendingTopic;

    SpellChecker *spellChecker;
    TextProcessor *textProcessor;
    CourseSearchEngine *searchEngine;
    QVector<CourseDocument> loadedCourses;
    bool indexReady = false;

    void loadCoursesFromStepik();
    void buildSearchIndex();

    QNetworkReply *currentLongPollReply = nullptr;
};

#endif // MONITORINGWORKER_H