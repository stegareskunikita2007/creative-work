#include "coursesearchengine.h"
#include <QtMath>
#include <QSet>

CourseSearchEngine::CourseSearchEngine(QObject *parent) : QObject(parent) {}

void CourseSearchEngine::buildIndex(const QVector<CourseDocument> &courses)
{
    documents = courses;
    if (documents.isEmpty()) return;

    int N = documents.size();
    QMap<QString, int> docCount;
    QSet<QString> allTerms;

    for (const auto &doc : documents) {
        QSet<QString> uniqueTerms;
        for (const auto &lemma : doc.lemmas) {
            uniqueTerms.insert(lemma);
            allTerms.insert(lemma);
        }
        for (const auto &term : uniqueTerms) {
            docCount[term]++;
        }
    }

    for (const auto &term : allTerms) {
        int n = docCount.value(term, 1);
        idf[term] = qLn(static_cast<double>(N) / n);
    }

    tfidfVectors.clear();
    for (const auto &doc : documents) {
        tfidfVectors.append(computeTFIDFVector(doc.lemmas));
    }
}

QMap<QString, double> CourseSearchEngine::computeTFIDFVector(const QStringList &lemmas) const
{
    QMap<QString, double> vec;
    if (lemmas.isEmpty()) return vec;

    QMap<QString, int> tfCount;
    for (const auto &lemma : lemmas) tfCount[lemma]++;

    int total = lemmas.size();
    for (auto it = tfCount.begin(); it != tfCount.end(); ++it) {
        double tf = static_cast<double>(it.value()) / total;
        double idfVal = idf.value(it.key(), 0.0);
        vec[it.key()] = tf * idfVal;
    }
    return vec;
}

double CourseSearchEngine::cosineSimilarity(const QMap<QString, double> &a,
                                            const QMap<QString, double> &b) const
{
    double dotProduct = 0.0, normA = 0.0, normB = 0.0;
    for (auto it = a.begin(); it != a.end(); ++it) {
        dotProduct += it.value() * b.value(it.key(), 0.0);
        normA += it.value() * it.value();
    }
    for (auto it = b.begin(); it != b.end(); ++it) {
        normB += it.value() * it.value();
    }
    if (normA == 0.0 || normB == 0.0) return 0.0;
    return dotProduct / (qSqrt(normA) * qSqrt(normB));
}

QPair<int, double> CourseSearchEngine::findBestCourse(const QStringList &queryLemmas) const
{
    if (documents.isEmpty() || queryLemmas.isEmpty())
        return {-1, 0.0};

    QMap<QString, double> queryVec = computeTFIDFVector(queryLemmas);

    int bestIndex = -1;
    double bestSimilarity = -1.0;
    for (int i = 0; i < tfidfVectors.size(); i++) {
        double sim = cosineSimilarity(queryVec, tfidfVectors[i]);
        if (sim > bestSimilarity) {
            bestSimilarity = sim;
            bestIndex = i;
        }
    }
    return {bestIndex, bestSimilarity};
}