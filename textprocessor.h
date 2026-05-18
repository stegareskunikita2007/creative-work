#ifndef TEXTPROCESSOR_H
#define TEXTPROCESSOR_H

#include <QObject>
#include <QString>
#include <QStringList>

class TextProcessor : public QObject
{
    Q_OBJECT
public:
    explicit TextProcessor(QObject *parent = nullptr);
    QStringList lemmatize(const QString &text); // возвращает леммы без стоп-слов

private:
    QStringList stopWords;
    QString mystemPath;
};

#endif // TEXTPROCESSOR_H