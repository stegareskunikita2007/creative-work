#ifndef SPELLCHECKER_H
#define SPELLCHECKER_H

#include <QObject>
#include <QString>
#include <QProcess>

class SpellChecker : public QObject
{
    Q_OBJECT
public:
    explicit SpellChecker(QObject *parent = nullptr);
    QString correctText(const QString &text);

private:
    QString dicPath;
};

#endif // SPELLCHECKER_H