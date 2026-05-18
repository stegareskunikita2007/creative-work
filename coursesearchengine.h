#ifndef COURSESEARCHENGINE_H
#define COURSESEARCHENGINE_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QStringList>

struct CourseDocument {
    int id;
    QString title;
    QString url;
    QStringList lemmas;  // леммы названия + описания
};

class CourseSearchEngine : public QObject
{
    Q_OBJECT
public:
    explicit CourseSearchEngine(QObject *parent = nullptr);
    void buildIndex(const QVector<CourseDocument> &courses);
    QPair<int, double> findBestCourse(const QStringList &queryLemmas) const;

private:
    QVector<CourseDocument> documents;
    QMap<QString, double> idf;
    QVector<QMap<QString, double>> tfidfVectors;

    QMap<QString, double> computeTFIDFVector(const QStringList &lemmas) const;
    double cosineSimilarity(const QMap<QString, double> &a,
                            const QMap<QString, double> &b) const;
};

#endif // COURSESEARCHENGINE_H