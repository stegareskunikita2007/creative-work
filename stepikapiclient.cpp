#include "stepikapiclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

StepikApiClient::StepikApiClient(QObject *parent)
    : QObject(parent)
{
    networkManager = new QNetworkAccessManager(this);
}

void StepikApiClient::searchCourses(const QString &query)
{
    QString urlString = QString(
                            "https://stepik.org/api/courses?search=%1&page=1"
                            ).arg(QUrl::toPercentEncoding(query));

    QUrl url(urlString);
    QNetworkRequest request;
    request.setUrl(url);

    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("Ошибка сети: " + reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        QJsonArray coursesArray = root["courses"].toArray();

        QVector<StepikCourse> courses;

        for (const QJsonValue &val : coursesArray) {
            QJsonObject obj = val.toObject();

            // Только бесплатные
            if (obj["is_paid"].toBool(false)) {
                continue;
            }

            // Только на русском
            if (obj["language"].toString() != "ru") {
                continue;
            }

            StepikCourse course;
            course.id = obj["id"].toInt();
            course.title = obj["title"].toString();
            course.url = QString("https://stepik.org/course/%1").arg(course.id);
            courses.append(course);
        }

        emit coursesReady(courses);
    });
}