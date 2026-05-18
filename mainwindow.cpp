#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->startBtn, &QPushButton::clicked,
            this, &MainWindow::onStartClicked);
    connect(ui->stopBtn, &QPushButton::clicked,
            this, &MainWindow::onStopClicked);
    connect(ui->checkConnectionBtn, &QPushButton::clicked,
            this, &MainWindow::onCheckConnectionClicked);

    worker = new MonitoringWorker(this);

    connect(worker, &MonitoringWorker::logMessage,
            [this](const QString &msg, const QString &type)
            {
                QString color;
                if (type == "error")      color = "red";
                else if (type == "success") color = "green";
                else if (type == "warning") color = "orange";
                else if (type == "debug")   color = "gray";
                else                       color = "white";

                ui->logsOutput->append(
                    QString("<span style='color:%1;'>%2</span>").arg(color, msg));
            });

    QString savedToken = loadToken();
    if (!savedToken.isEmpty()) {
        ui->vkTokenInput->setText(savedToken);
        ui->logsOutput->append("<span style='color:gray;'>📂 Токен VK загружен из сохранённых настроек</span>");
    }

    connect(ui->forgetTokenBtn, &QPushButton::clicked,
            this, &MainWindow::onForgetTokenClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::saveToken(const QString &token)
{
    QSettings settings("MurrkyWorks", "StudBot");
    settings.setValue("vk/token", token);
}

QString MainWindow::loadToken()
{
    QSettings settings("MurrkyWorks", "StudBot");
    return settings.value("vk/token", "").toString();
}

void MainWindow::onForgetTokenClicked()
{
    QSettings settings("MurrkyWorks", "StudBot");
    settings.remove("vk/token");

    ui->vkTokenInput->clear();
    ui->logsOutput->append("<span style='color:orange;'>🗑 Токен удалён из сохранённых настроек</span>");
}

void MainWindow::onStartClicked()
{
    QString token = ui->vkTokenInput->text().trimmed();

    if (token.isEmpty()) {
        ui->logsOutput->append(
            "<span style='color:red;'>❌ Введите токен VK!</span>");
        return;
    }

    ui->logsOutput->append(
        "<span style='color:green;'>▶ Бот запущен</span>");
    ui->botStatus->setText("Статус: Активен");
    ui->startBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);

    worker->setToken(token);
    worker->setGroupId("238530979");
    worker->start();

    saveToken(token);
}

void MainWindow::onStopClicked()
{
    worker->stop();
    ui->logsOutput->append(
        "<span style='color:orange;'>⏹ Мониторинг остановлен</span>");
    ui->botStatus->setText("Статус: Остановлен");
    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
}

void MainWindow::onCheckConnectionClicked()
{
    QString token = ui->vkTokenInput->text().trimmed();
    if (token.isEmpty()) {
        ui->logsOutput->append(
            "<span style='color:red;'>❌ Введите токен для проверки</span>");
        return;
    }

    // Проверка соединения через запрос к API
    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    QString url = QString(
                      "https://api.vk.com/method/groups.getById?"
                      "access_token=%1&v=5.199").arg(token);

    QNetworkRequest request;
    request.setUrl(QUrl(url));

    QNetworkReply *reply = mgr->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, mgr]() {
        reply->deleteLater();
        mgr->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            ui->logsOutput->append(
                "<span style='color:red;'>❌ Ошибка соединения: "
                + reply->errorString() + "</span>");
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        if (root.contains("error")) {
            ui->logsOutput->append(
                "<span style='color:red;'>❌ Ошибка VK: "
                + root["error"].toObject()["error_msg"].toString() + "</span>");
        } else {
            ui->logsOutput->append(
                "<span style='color:green;'>✅ Соединение с VK API установлено</span>");
        }
    });
}