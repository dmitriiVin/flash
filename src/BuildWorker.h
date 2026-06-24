#pragma once

#include <QObject>
#include <QString>

struct BuildRequest {
    int diskNumber = -1;
    QString winPeSource;
    QString installerLauncher;
    QString installerAgent;
    QString configJson;
    QString softwareDirectory;
    QString driversDirectory;
    QString installEsd;
};

class BuildWorker : public QObject {
    Q_OBJECT

  public:
    explicit BuildWorker(BuildRequest request, QObject *parent = nullptr);

  public slots:
    void Run();

  signals:
    void LogMessage(const QString &message);
    void Finished(bool success, const QString &message);

  private:
    BuildRequest request_;
};
