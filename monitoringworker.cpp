#include "monitoringworker.h"
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include "spellchecker.h"

MonitoringWorker::MonitoringWorker(QObject *parent)
    : QObject(parent)
{

    networkManager = new QNetworkAccessManager(this);
    stepikClient = new StepikApiClient(this);
    textProcessor = new TextProcessor(this);
    searchEngine = new CourseSearchEngine(this);

    connect(stepikClient, &StepikApiClient::coursesReady,
            this, &MonitoringWorker::onStepikCoursesReady);
    connect(stepikClient, &StepikApiClient::errorOccurred,
            this, [this](const QString &err) {
                emit logMessage("❌ Stepik API: " + err, "error");
            });

    spellChecker = new SpellChecker(this);
}

// ─── Токен и группа ───────────────────────────────────────

void MonitoringWorker::setToken(const QString &token)
{
    vkToken = token;
    emit logMessage("✅ Токен VK сохранён", "info");
}

void MonitoringWorker::setGroupId(const QString &id)
{
    groupId = id;
    emit logMessage("✅ Group ID установлен: " + id, "info");
}

// ─── Управление ботом ─────────────────────────────────────

void MonitoringWorker::start()
{
    isRunning = true;
    processedEventIds.clear();
    emit logMessage("Бот запущен. Загружаю курсы со Stepik...", "info");
    loadCoursesFromStepik();
}

void MonitoringWorker::stop()
{
    isRunning = false;
    emit logMessage("Бот остановлен", "warning");
}

// ─── Long Poll ────────────────────────────────────────────

void MonitoringWorker::getLongPollServer()
{
    QString url = QString(
                      "https://api.vk.com/method/groups.getLongPollServer?"
                      "access_token=%1&v=5.199&group_id=%2"
                      ).arg(vkToken).arg(groupId);

    QNetworkRequest request;
    request.setUrl(QUrl(url));

    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit logMessage("Ошибка Long Poll: " + reply->errorString(), "error");
            return;
        }

        QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();

        if (root.contains("error")) {
            emit logMessage("Ошибка VK: " + root["error"].toObject()["error_msg"].toString(), "error");
            return;
        }

        QJsonObject response = root["response"].toObject();
        longPollServer = response["server"].toString();
        longPollKey   = response["key"].toString();
        longPollTs    = response["ts"].toString();

        emit logMessage("Сервер Long Poll получен", "success");
        startLongPoll();
    });
}

// ─── Long Poll ────────────────────────────────────────────

void MonitoringWorker::startLongPoll()
{
    if (!isRunning) return;

    QString url = QString("%1?act=a_check&key=%2&ts=%3&wait=25")
                      .arg(longPollServer, longPollKey, longPollTs);

    QNetworkRequest request;
    request.setUrl(QUrl(url));

    QNetworkReply *reply = networkManager->get(request);
    currentLongPollReply = reply;   // ← добавить

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (currentLongPollReply == reply) {
            currentLongPollReply = nullptr;
        }

        if (!isRunning) return;

        if (reply->error() != QNetworkReply::NoError) {
            emit logMessage("Ошибка сети: " + reply->errorString(), "error");
            QTimer::singleShot(3000, this, &MonitoringWorker::startLongPoll);
            return;
        }

        QByteArray responseData = reply->readAll();

        // logs
        //emit logMessage("Long Poll ответ: " + QString(responseData), "debug");

        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject root = doc.object();

        // Проверка: сервер требует переподключения
        if (root.contains("failed")) {
            int code = root["failed"].toInt();

            if (code == 1) {
                // Просто обновляем ts и продолжаем без перезапуска
                if (root.contains("ts")) {
                    longPollTs = QString::number(root["ts"].toInt());
                    emit logMessage("Long Poll ts устарел, новый ts: " + longPollTs, "warning");
                }
                if (isRunning) {
                    QTimer::singleShot(100, this, &MonitoringWorker::startLongPoll);
                }
            } else {
                emit logMessage(QString("Long Poll failed (код %1). Перезапуск сервера...").arg(code), "warning");
                QTimer::singleShot(2000, this, &MonitoringWorker::getLongPollServer);
            }
            return;
        }

        // Сохраняем ts для следующего запроса
        if (root.contains("ts")) {
            longPollTs = root["ts"].toString();
        }

        // Обрабатываем события
        processLongPollResponse(root);

        // Следующий цикл
        if (isRunning) {
            QTimer::singleShot(100, this, &MonitoringWorker::startLongPoll);
        }
    });
}

void MonitoringWorker::processLongPollResponse(const QJsonObject &data)
{
    if (!data.contains("updates")) {
        return;
    }

    QJsonArray updates = data["updates"].toArray();

    for (const QJsonValue &update : updates) {
        QJsonObject updateObj = update.toObject();

        QString type = updateObj["type"].toString();
        if (type != "message_new") {
            continue;
        }

        QString eventId = updateObj["event_id"].toString();
        if (processedEventIds.contains(eventId)) {
            emit logMessage("⏭ Событие уже обработано, пропускаю", "debug");
            continue;
        }

        QJsonObject object = updateObj["object"].toObject();
        QJsonObject message = object["message"].toObject();

        int peerId = message["peer_id"].toInt();
        int fromId = message["from_id"].toInt();
        int out = message.value("out").toInt(0);
        QString text = message["text"].toString();

        if (out != 0) {
            emit logMessage(QString("📤 Исходящее от бота (peer_id=%1) — пропущено").arg(peerId), "debug");
            continue;
        }

        if (text.isEmpty()) {
            emit logMessage("⏭ Сообщение с пустым текстом, пропускаю", "debug");
            continue;
        }

        QString preview = text.left(60).replace('\n', ' ');
        emit logMessage(QString("📩 Чат %1 от id%2: «%3»")
                            .arg(peerId).arg(fromId).arg(preview), "info");

        // --- 1. Первое исправление опечаток (SpellChecker) ---
        QString correctedText = spellChecker->correctText(text);
        if (correctedText != text) {
            emit logMessage("🔧 После первого исправления: «" + correctedText + "»", "debug");
        }

        // --- 2. Проверка маркера помощи в ИСПРАВЛЕННОМ тексте ---
        QString lower = correctedText.toLower();
        QStringList helpMarkers = {
            "помогите", "не понимаю", "объясните", "сложно", "не могу", "запутался",
            "нужна помощь", "помочь", "выручайте", "подскажите", "кто знает",
            "кто шарит", "кто разбирается", "не получается", "не работает",
            "не компилируется", "не знаю", "сложна", "трудно", "непонятно"
        };

        bool needsHelp = false;
        for (const auto &marker : helpMarkers) {
            if (lower.contains(marker)) {
                needsHelp = true;
                break;
            }
        }

        if (!needsHelp) {
            emit logMessage("⏭ Нет маркера помощи", "debug");
            continue;
        }

        emit logMessage("✅ Найден маркер помощи", "info");

        // --- 3. Лемматизация ---
        correctedText.replace("C++", "cpp", Qt::CaseInsensitive);
        QStringList queryLemmas = textProcessor->lemmatize(correctedText);
        if (queryLemmas.isEmpty()) {
            emit logMessage("⏭ Не удалось выделить значимые слова", "debug");
            continue;
        }

        emit logMessage("🔑 Леммы после лемматизации: " + queryLemmas.join(' '), "info");

        // --- 4. Второе исправление опечаток (для лемм) ---
        QString lemmasText = queryLemmas.join(' ');
        QString correctedLemmas = spellChecker->correctText(lemmasText);
        if (correctedLemmas != lemmasText) {
            emit logMessage("🔧 После второго исправления: «" + correctedLemmas + "»", "debug");
            queryLemmas = correctedLemmas.split(' ', Qt::SkipEmptyParts);
        }

        // --- 5. Поиск лучшего курса по TF-IDF ---
        if (!indexReady) {
            emit logMessage("⏳ Индекс ещё не готов, пропускаю", "warning");
            continue;
        }

        auto [bestIndex, similarity] = searchEngine->findBestCourse(queryLemmas);

        if (bestIndex < 0 || similarity < 0.25) {
            emit logMessage("❌ Подходящий курс не найден", "warning");
            continue;
        }

        const auto &course = loadedCourses[bestIndex];
        emit logMessage(QString("✅ Найден курс: «%1» (сходство: %2)")
                            .arg(course.title).arg(similarity, 0, 'f', 3), "success");

        // --- 6. Отправка рекомендации ---
        QString response = QString("Рекомендую курс: «%1» %2").arg(course.title, course.url);
        sendMessage(peerId, response);

        processedEventIds.insert(eventId);
    }
}

// ─── Отправка сообщения ───────────────────────────────────

void MonitoringWorker::sendMessage(int peerId, const QString &message)
{
    int delay = QRandomGenerator::global()->bounded(1000, 2000);
    emit logMessage(QString("⏳ Ответ будет отправлен через %1 сек...").arg(delay / 1000), "debug");

    QTimer::singleShot(delay, this, [this, peerId, message]() {
        // Готовим данные для POST-запроса
        QUrlQuery postData;
        postData.addQueryItem("access_token", vkToken);
        postData.addQueryItem("v", "5.199");
        postData.addQueryItem("peer_id", QString::number(peerId));
        postData.addQueryItem("message", QUrl::toPercentEncoding(message)); // Не кодируем, Qt сделает сам
        postData.addQueryItem("random_id", QString::number(QRandomGenerator::global()->generate64() & 0x7FFFFFFFFFFFFFFF));
        postData.addQueryItem("group_id", groupId);

        // Формируем запрос
        QUrl url("https://api.vk.com/method/messages.send");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        // Отправляем POST
        QNetworkReply *reply = networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

        connect(reply, &QNetworkReply::finished, this, [this, reply, peerId]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                emit logMessage("Ошибка отправки: " + reply->errorString(), "error");
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject root = doc.object();

            if (root.contains("error")) {
                emit logMessage("Ошибка VK: " + root["error"].toObject()["error_msg"].toString(), "error");
            } else {
                emit logMessage("✅ Ответ отправлен в чат " + QString::number(peerId), "success");
            }
        });
    });
}

void MonitoringWorker::onStepikCoursesReady(const QVector<StepikCourse> &courses)
{
    if (courses.isEmpty()) {
        emit logMessage("❌ Курсы не найдены", "warning");
        return;
    }

    const StepikCourse &course = courses.first();

    emit logMessage(QString("✅ Найдено %1 курсов, первый: %2")
                        .arg(courses.size())
                        .arg(course.title), "success");

    QString response = QString(
                           "Здравствуйте! Рекомендую курс по вашему вопросу: «%1» %2"
                           ).arg(course.title, course.url);

    sendMessage(pendingPeerId, response);
}

void MonitoringWorker::loadCoursesFromStepik()
{
    QStringList topics = {
        "C++", "Python", "Java",
        "алгоритмы", "алгоритмы и структуры данных",
        "структуры данных",
        "интегралы", "производные", "математический анализ",
        "линейная алгебра", "дискретная математика",
        "SQL", "базы данных",
        "машинное обучение", "нейронные сети",
        "дифференциальные уравнения",
        "теория вероятностей", "статистика",
        "операционные системы", "компьютерные сети",
        "веб-разработка", "мобильная разработка"
    };

    loadedCourses.clear();
    int pagesPerTopic = 1; // уменьшим до 2 страниц, чтобы быстрее загрузить
    // Строим очередь запросов
    QStringList requestUrls;
    for (const auto &topic : topics) {
        QString encoded = topic;
        encoded.replace('+', "%2B"); // ручная замена
        for (int page = 1; page <= pagesPerTopic; ++page) {
            requestUrls.append(QString("https://stepik.org/api/courses?search=%1&page=%2")
                                   .arg(encoded)
                                   .arg(page));
        }
    }

    int totalRequests = requestUrls.size();
    auto *pending = new QAtomicInt(totalRequests);
    auto *urlQueue = new QStringList(requestUrls);
    auto *timer = new QTimer(this);
    auto *networkManager = this->networkManager; // предполагаем, что networkManager уже есть в классе

    // Таймаут общего ожидания — 40 секунд
    QTimer::singleShot(40000, this, [this, pending, timer, urlQueue]() {
        if (pending->loadRelaxed() > 0) {
            emit logMessage("⚠️ Stepik API не ответил на все запросы, продолжаю с тем что есть", "warning");
            pending->storeRelaxed(0);
            timer->stop();
            buildSearchIndex();
        }
        delete urlQueue;
        delete pending;
    });

    connect(timer, &QTimer::timeout, this, [=]() mutable {
        if (urlQueue->isEmpty()) {
            timer->stop();
            return;
        }
        QString url = urlQueue->takeFirst();
        emit logMessage("🔍 Запрос к Stepik: " + url, "debug");

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        QNetworkReply *reply = networkManager->get(request);

        connect(reply, &QNetworkReply::finished, this, [this, reply, pending]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                emit logMessage("❌ Ошибка Stepik: " + reply->errorString(), "error");
            } else {
                QByteArray data = reply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data);
                QJsonArray courses = doc.object()["courses"].toArray();

                emit logMessage(QString("📥 Получено %1 курсов").arg(courses.size()), "debug");

                for (const auto &val : courses) {
                    QJsonObject obj = val.toObject();
                    if (obj["is_paid"].toBool(false)) continue;
                    if (obj["language"].toString() != "ru") continue;

                    CourseDocument courseDoc;
                    courseDoc.id = obj["id"].toInt();
                    courseDoc.title = obj["title"].toString();
                    courseDoc.url = QString("https://stepik.org/course/%1").arg(courseDoc.id);
                    courseDoc.lemmas = textProcessor->lemmatize(
                        courseDoc.title + " " + obj["summary"].toString());

                    if (!courseDoc.lemmas.isEmpty()) {
                        loadedCourses.append(courseDoc);
                    }
                }
            }

            int remaining = pending->fetchAndAddRelaxed(-1) - 1;
            if (remaining == 0) {
                emit logMessage("✅ Все запросы к Stepik завершены", "info");
                buildSearchIndex();
            }
        });
    });

    timer->start(300); // запрос каждые 300 мс
}

void MonitoringWorker::buildSearchIndex()
{
    // ── Фиксированные курсы (гарантированно будут в индексе) ──
    QVector<CourseDocument> fixedCourses;

    fixedCourses.append({
        0,
        "Программирование на C++",
        "https://stepik.org/course/7",
        textProcessor->lemmatize("Программирование на C++ cpp язык программирования")
    });

    fixedCourses.append({
        0,
        "Python для начинающих",
        "https://stepik.org/course/67",
        textProcessor->lemmatize("Python для начинающих python питон программирование язык")
    });

    fixedCourses.append({
        0,
        "Java для начинающих",
        "https://stepik.org/course/187",
        textProcessor->lemmatize("Java для начинающих java язык программирования")
    });

    fixedCourses.append({
        0,
        "Основы SQL",
        "https://stepik.org/course/551",
        textProcessor->lemmatize("SQL базы данных запросы select join")
    });

    fixedCourses.append({
        0,
        "Алгоритмы и структуры данных",
        "https://stepik.org/course/1547",
        textProcessor->lemmatize("алгоритмы структуры данных сортировка графы бинарный поиск сложность деревья хеш-таблицы")
    });

    // Добавляем фиксированные курсы, если их ещё нет
    for (const auto &fc : fixedCourses) {
        bool exists = false;
        for (const auto &lc : loadedCourses) {
            if (lc.url == fc.url) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            loadedCourses.append(fc);
            emit logMessage("📌 Добавлен фиксированный курс: " + fc.title, "info");
        }
    }

    emit logMessage(QString("Загружено %1 курсов. Строю поисковый индекс...")
                        .arg(loadedCourses.size()), "info");

    searchEngine->buildIndex(loadedCourses);
    indexReady = true;

    emit logMessage("✅ Поисковый индекс готов!", "success");

    getLongPollServer();
}