#ifndef STEPIKAPICLIENT_H
#define STEPIKAPICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

struct StepikCourse {
    int id;
    QString title;
    QString url;
};

class StepikApiClient : public QObject
{
    Q_OBJECT

public:
    explicit StepikApiClient(QObject *parent = nullptr);
    void searchCourses(const QString &query);

private:
    QNetworkAccessManager *networkManager;

signals:
    void coursesReady(const QVector<StepikCourse> &courses);
    void errorOccurred(const QString &error);
};

#endif // STEPIKAPICLIENT_H