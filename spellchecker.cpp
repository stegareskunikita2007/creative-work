#include "spellchecker.h"
#include <QCoreApplication>
#include <QFile>

SpellChecker::SpellChecker(QObject *parent)
    : QObject(parent)
{
    dicPath = QCoreApplication::applicationDirPath() + "/frequency_dictionary_ru.txt";
    if (!QFile::exists(dicPath))
        dicPath = "frequency_dictionary_ru.txt";
}

QString SpellChecker::correctText(const QString &text)
{
    QStringList words = text.split(' ', Qt::SkipEmptyParts);
    if (words.isEmpty()) return text;

    QStringList args;
    args << dicPath;        // путь к словарю
    args.append(words);      // слова для исправления

    QProcess process;
    process.start(QCoreApplication::applicationDirPath() + "/spellchecker_cli.exe", args);
    process.waitForFinished(5000);

    if (process.exitCode() != 0)
        return text;

    // Читаем вывод как UTF-8
    QByteArray output = process.readAllStandardOutput();

    // Заменяем все '?' на пробелы (разделитель слов в выводе)
    QString corrected = QString::fromUtf8(output).trimmed();

    // Убираем перевод строки, если есть
    corrected.replace('\n', ' ').replace('\r', ' ');

    return corrected.isEmpty() ? text : corrected;
}