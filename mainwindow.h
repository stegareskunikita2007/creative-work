#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QMessageBox>
#include "monitoringworker.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onCheckConnectionClicked();
    void onForgetTokenClicked();

private:
    Ui::MainWindow *ui;
    MonitoringWorker *worker;

    void saveToken(const QString &token);
    QString loadToken();
};

#endif // MAINWINDOW_H