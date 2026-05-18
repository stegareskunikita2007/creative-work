#include "textprocessor.h"
#include <QProcess>
#include <QRegularExpression>

TextProcessor::TextProcessor(QObject *parent) : QObject(parent)
{
#ifdef Q_OS_WIN
    mystemPath = "./mystem.exe";
#else
    mystemPath = "./mystem";
#endif

    stopWords = {
        // Падежные формы, местоимения, предлоги, союзы
        "и", "в", "на", "с", "по", "к", "у", "о", "от", "из", "для",
        "как", "что", "это", "он", "она", "они", "мы", "вы", "я", "ты",
        "а", "но", "или", "бы", "же", "ли", "то", "так", "уже", "ещё",
        "весь", "мой", "твой", "свой", "его", "её", "их", "наш", "ваш",
        "не", "нет", "да", "очень", "совсем", "вот", "там", "тут",

        // Маркеры просьб о помощи (их уберём, чтобы не мешали поиску)
        "помогите", "помочь", "помогать", "разобраться", "объясните",
        "подскажите", "пожалуйста", "нужна", "нужно", "надо", "могу",
        "может", "можешь", "мочь", "понимаю", "понимать", "запутался",
        "сложно", "трудно", "выручайте", "спасите", "помогити",
        "помоги", "помог", "объяснять", "объясните", "объясни",

        // Разговорный мусор, вводные слова
        "блин", "вообще", "короче", "типа", "ну", "кароч", "кстати",
        "кто", "кто-нибудь", "кто-то", "что-то", "где", "зачем", "почему",
        "какой", "какая", "какое", "какие", "какого", "какому", "каким",
        "когда", "сколько", "чей", "чья", "чьё", "чьи",
        "всё", "все", "вся", "всех", "всем", "всеми",
        "вообще-то", "типа-того", "как-то", "где-то", "что-нибудь",
        "чё", "чёта", "чота", "щас", "сейчас", "потом",
        "реально", "конкретно", "просто", "тупо", "вообще-то",
        "тип", "типо", "эт", "это", "этот", "эта", "эти", "этого",
        "чел", "чувак", "народ", "ребят", "ребята", "пацаны",
        "слушай", "слушайте", "смотри", "смотрите", "глянь", "гляньте",

        // Дополнительные стоп-слова, которых не было раньше
        "более", "менее", "оч", "очень", "довольно", "такой", "такая",
        "такое", "такие", "тоже", "также", "вроде", "вроде-бы",
        "допустим", "например", "значит", "кроме", "ладно",
        "во-первых", "во-вторых", "наконец", "следовательно",
        "собственно", "сорян", "извините", "простите", "плиз",
        "хелп", "help", "помощь", "помощи", "помощью",
        "нужна", "нужен", "нужны", "надо", "надобно", "над",
        "будь", "будьте", "давай", "давайте", "здравствовать", "здравствуйте"
    };
}

QStringList TextProcessor::lemmatize(const QString &text)
{
    QString cleaned = text;
    // Временно заменяем "c++" на "cpp", чтобы плюсы не удалялись
    cleaned.replace("c++", "cpp");
    cleaned.remove(QRegularExpression("[!\"#$%&'()*+,-./:;<=>?@\\[\\]^_`{|}~]"));
    cleaned = cleaned.toLower().trimmed();

    if (cleaned.isEmpty()) return {};

    QProcess process;
    process.start(mystemPath, {"-n", "-l", "--format", "text"});
    if (!process.waitForStarted(2000)) {
        // fallback — возвращаем слова длиннее 3 символов
        QStringList words = cleaned.split(' ', Qt::SkipEmptyParts);
        QStringList result;
        for (const auto &w : words) {
            if (w.length() >= 3 && !stopWords.contains(w))
                result.append(w);
        }
        return result;
    }

    process.write(cleaned.toUtf8());
    process.closeWriteChannel();
    process.waitForFinished(3000);

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList lemmas;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QString lemma = line.trimmed();
        if (lemma.isEmpty() || lemma == "?") continue;

        // Удаляем фигурные скобки и вопросительные знаки
        lemma.remove('{').remove('}').remove('?');

        // MyStem может вернуть несколько вариантов через '|', берём первый
        if (lemma.contains('|')) {
            lemma = lemma.split('|').first().trimmed();
        }

        if (lemma.length() >= 3 && !stopWords.contains(lemma)) {
            lemmas.append(lemma);
        }
    }
    return lemmas;
}