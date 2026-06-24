#include "MainWindow.h"

#include "WindowsUtils.h"

#include <QApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>

#include <filesystem>

namespace
{
QString FormatBytes(uint64_t bytes) {
    constexpr double kib = 1024.0;
    constexpr double mib = kib * 1024.0;
    constexpr double gib = mib * 1024.0;

    if (bytes >= static_cast<uint64_t>(gib)) {
        return QString::number(static_cast<double>(bytes) / gib, 'f', 1) + " GB";
    }

    if (bytes >= static_cast<uint64_t>(mib)) {
        return QString::number(static_cast<double>(bytes) / mib, 'f', 1) + " MB";
    }

    return QString::number(bytes) + " bytes";
}

bool DirectoryExists(const QString &path) {
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(path.toStdWString()), ec);
}

bool FileExists(const QString &path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(path.toStdWString()), ec);
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("UsbBuilder");

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);

    auto *deviceGroup = new QGroupBox("USB devices", central);
    auto *deviceLayout = new QVBoxLayout(deviceGroup);
    auto *deviceHeader = new QHBoxLayout();
    deviceHeader->addWidget(new QLabel("Select target disk:", deviceGroup));
    deviceHeader->addStretch();
    refreshButton_ = new QPushButton("Refresh", deviceGroup);
    deviceHeader->addWidget(refreshButton_);
    deviceLayout->addLayout(deviceHeader);

    deviceList_ = new QListWidget(deviceGroup);
    deviceList_->setMinimumHeight(110);
    deviceLayout->addWidget(deviceList_);
    rootLayout->addWidget(deviceGroup);

    auto *sourceGroup = new QGroupBox("Sources", central);
    auto *form = new QFormLayout(sourceGroup);
    winPeSourceEdit_ = AddPathRow(form, "WinPE source:", "Browse", this, SLOT(BrowseWinPeSource()));
    installerLauncherEdit_ = AddPathRow(form, "InstallerLauncher:", "Browse", this, SLOT(BrowseInstallerLauncher()));
    installerAgentEdit_ = AddPathRow(form, "InstallerAgent:", "Browse", this, SLOT(BrowseInstallerAgent()));
    configJsonEdit_ = AddPathRow(form, "config.json:", "Browse", this, SLOT(BrowseConfigJson()));
    softwareDirectoryEdit_ = AddPathRow(form, "Software directory:", "Browse", this, SLOT(BrowseSoftwareDirectory()));
    driversDirectoryEdit_ = AddPathRow(form, "Drivers directory:", "Browse", this, SLOT(BrowseDriversDirectory()));
    installEsdEdit_ = AddPathRow(form, "Install.esd:", "Browse", this, SLOT(BrowseInstallEsd()));
    rootLayout->addWidget(sourceGroup);

    logEdit_ = new QTextEdit(central);
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(140);
    rootLayout->addWidget(logEdit_);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    createButton_ = new QPushButton("Create USB", central);
    createButton_->setMinimumWidth(140);
    buttonLayout->addWidget(createButton_);
    rootLayout->addLayout(buttonLayout);

    setCentralWidget(central);
    resize(760, 620);

    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::RefreshDevices);
    connect(createButton_, &QPushButton::clicked, this, &MainWindow::CreateUsb);

    RefreshDevices();
}

QLineEdit *MainWindow::AddPathRow(QFormLayout *form, const QString &label, const QString &buttonText, const QObject *receiver, const char *member) {
    auto *row = new QWidget(this);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *edit = new QLineEdit(row);
    auto *button = new QPushButton(buttonText, row);
    layout->addWidget(edit, 1);
    layout->addWidget(button);

    connect(button, SIGNAL(clicked()), receiver, member);
    form->addRow(label, row);
    return edit;
}

void MainWindow::RefreshDevices() {
    deviceList_->clear();
    devices_ = EnumerateUsbDevices();

    for (const auto &device : devices_) {
        const QString title = QString::fromUtf8(device.model.c_str()) + " " + FormatBytes(device.sizeBytes);
        auto *item = new QListWidgetItem(title, deviceList_);
        item->setData(Qt::UserRole, device.number);
        item->setToolTip("PhysicalDrive" + QString::number(device.number));
    }

    if (devices_.empty()) {
        auto *item = new QListWidgetItem("No removable USB disks found", deviceList_);
        item->setFlags(Qt::NoItemFlags);
    }
}

void MainWindow::CreateUsb() {
    BuildRequest request;
    if (!ValidateInputs(request)) {
        return;
    }

    if (!WindowsUtils::IsProcessElevated()) {
        QMessageBox::warning(this, "Administrator Required", "UsbBuilder must be run as Administrator to prepare disks.");
        return;
    }

    const auto answer = QMessageBox::warning(this, "Confirm USB Erase", "All data on the selected device will be deleted.\nContinue?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    logEdit_->clear();
    SetBusy(true);

    auto *thread = new QThread(this);
    auto *worker = new BuildWorker(request);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &BuildWorker::Run);
    connect(worker, &BuildWorker::LogMessage, this, &MainWindow::AppendLog);
    connect(worker, &BuildWorker::Finished, this, [this, thread, worker](bool success, const QString &message) {
        AppendLog(message);
        SetBusy(false);

        if (success) {
            QMessageBox::information(this, "UsbBuilder", message);
            RefreshDevices();
        }
        else {
            QMessageBox::critical(this, "UsbBuilder", message);
        }
    });
    connect(worker, &BuildWorker::Finished, thread, &QThread::quit);
    connect(worker, &BuildWorker::Finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MainWindow::BrowseWinPeSource() {
    const QString path = QFileDialog::getExistingDirectory(this, "Select WinPE Source");
    if (!path.isEmpty()) {
        winPeSourceEdit_->setText(path);
    }
}

void MainWindow::BrowseInstallerLauncher() {
    const QString path = QFileDialog::getOpenFileName(this, "Select InstallerLauncher", {}, "Executable (*.exe)");
    if (!path.isEmpty()) {
        installerLauncherEdit_->setText(path);
    }
}

void MainWindow::BrowseInstallerAgent() {
    const QString path = QFileDialog::getOpenFileName(this, "Select InstallerAgent", {}, "Executable (*.exe)");
    if (!path.isEmpty()) {
        installerAgentEdit_->setText(path);
    }
}

void MainWindow::BrowseConfigJson() {
    const QString path = QFileDialog::getOpenFileName(this, "Select config.json", {}, "JSON (*.json)");
    if (!path.isEmpty()) {
        configJsonEdit_->setText(path);
    }
}

void MainWindow::BrowseSoftwareDirectory() {
    const QString path = QFileDialog::getExistingDirectory(this, "Select Software Directory");
    if (!path.isEmpty()) {
        softwareDirectoryEdit_->setText(path);
    }
}

void MainWindow::BrowseDriversDirectory() {
    const QString path = QFileDialog::getExistingDirectory(this, "Select Drivers Directory");
    if (!path.isEmpty()) {
        driversDirectoryEdit_->setText(path);
    }
}

void MainWindow::BrowseInstallEsd() {
    const QString path = QFileDialog::getOpenFileName(this, "Select install.esd", {}, "Windows image (*.esd)");
    if (!path.isEmpty()) {
        installEsdEdit_->setText(path);
    }
}

void MainWindow::AppendLog(const QString &message) {
    logEdit_->append(message);
}

bool MainWindow::ValidateInputs(BuildRequest &request) {
    const auto *item = deviceList_->currentItem();
    if (item == nullptr || !item->data(Qt::UserRole).isValid()) {
        QMessageBox::warning(this, "UsbBuilder", "Select a USB device.");
        return false;
    }

    request.diskNumber = item->data(Qt::UserRole).toInt();
    request.winPeSource = winPeSourceEdit_->text().trimmed();
    request.installerLauncher = installerLauncherEdit_->text().trimmed();
    request.installerAgent = installerAgentEdit_->text().trimmed();
    request.configJson = configJsonEdit_->text().trimmed();
    request.softwareDirectory = softwareDirectoryEdit_->text().trimmed();
    request.driversDirectory = driversDirectoryEdit_->text().trimmed();
    request.installEsd = installEsdEdit_->text().trimmed();

    if (!DirectoryExists(request.winPeSource)) {
        QMessageBox::warning(this, "UsbBuilder", "Select a valid WinPE source directory.");
        return false;
    }

    if (!request.installerLauncher.isEmpty() && !FileExists(request.installerLauncher)) {
        QMessageBox::warning(this, "UsbBuilder", "Select a valid InstallerLauncher executable.");
        return false;
    }

    if (!FileExists(request.installerAgent)) {
        QMessageBox::warning(this, "UsbBuilder", "Select a valid InstallerAgent executable.");
        return false;
    }

    if (!FileExists(request.configJson)) {
        QMessageBox::warning(this, "UsbBuilder", "Select a valid config.json file.");
        return false;
    }

    if (!DirectoryExists(request.softwareDirectory)) {
        QMessageBox::warning(this, "UsbBuilder", "Select a valid Software directory.");
        return false;
    }

    if (!request.driversDirectory.isEmpty() && !DirectoryExists(request.driversDirectory)) {
        QMessageBox::warning(this, "UsbBuilder", "Select a valid Drivers directory.");
        return false;
    }

    if (!FileExists(request.installEsd)) {
        QMessageBox::warning(this, "UsbBuilder", "Select a valid install.esd file.");
        return false;
    }

    return true;
}

void MainWindow::SetBusy(bool busy) {
    deviceList_->setEnabled(!busy);
    refreshButton_->setEnabled(!busy);
    createButton_->setEnabled(!busy);
}
