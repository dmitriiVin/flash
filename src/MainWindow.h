#pragma once

#include "BuildWorker.h"
#include "UsbDeviceManager.h"

#include <QMainWindow>

#include <vector>

class QLineEdit;
class QListWidget;
class QPushButton;
class QTextEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = nullptr);

  private slots:
    void RefreshDevices();
    void CreateUsb();
    void BrowseWinPeSource();
    void BrowseInstallerLauncher();
    void BrowseInstallerAgent();
    void BrowseConfigJson();
    void BrowseSoftwareDirectory();
    void BrowseDriversDirectory();
    void BrowseInstallEsd();

  private:
    QLineEdit *AddPathRow(class QFormLayout *form, const QString &label, const QString &buttonText, const QObject *receiver, const char *member);
    void AppendLog(const QString &message);
    bool ValidateInputs(BuildRequest &request);
    void SetBusy(bool busy);

    QListWidget *deviceList_ = nullptr;
    QLineEdit *winPeSourceEdit_ = nullptr;
    QLineEdit *installerLauncherEdit_ = nullptr;
    QLineEdit *installerAgentEdit_ = nullptr;
    QLineEdit *configJsonEdit_ = nullptr;
    QLineEdit *softwareDirectoryEdit_ = nullptr;
    QLineEdit *driversDirectoryEdit_ = nullptr;
    QLineEdit *installEsdEdit_ = nullptr;
    QTextEdit *logEdit_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QPushButton *createButton_ = nullptr;

    std::vector<UsbDevice> devices_;
};
