#ifndef MODBUSCLIENT_H
#define MODBUSCLIENT_H

#include <QMainWindow>
#include <QModbusDevice>
#include <QQueue>

#include "Config.h"

#include "ModbusBase.h"

QT_BEGIN_NAMESPACE

class QModbusClient;
class QModbusReply;

namespace Ui { class fmodbus_client; }

QT_END_NAMESPACE


class ModbusClient : public QWidget, public ModbusBase {
    Q_OBJECT
public:
    ModbusClient(QWidget *parent = nullptr);
    ~ModbusClient();
signals:
    void read_ready(QModbusDataUnit mdu);
    void poll_timer_changed(int value);
public slots:
    int get_poll_timer() const;
    void on_connect_type(int);
    void on_connect();

    void on_read(QModbusDataUnit mdu);
    void sendSingleRequest(QModbusDataUnit mdu);
    void processNextRequest();
    void on_write(QModbusDataUnit mdu);
    void on_read(QModbusDataUnit::RegisterType type, int startAddress, quint16 numberOfEntries);
    void on_write(QModbusDataUnit::RegisterType type, int startAddress, quint16 numberOfEntries);

    void setRegister(QModbusDataUnit::RegisterType table, const QString &reg_name, double value);
    void setRegisterString(QModbusDataUnit::RegisterType table, const QString &reg_name, const QString &value);

//    void on_write(QString reg_name, unsigned int value);
//    void on_write(QString reg_name, double value);
    void read_config();
    void write_config();
private slots:
    void onModbusStateChanged(int state);
    void onModbusErrorOccurred(QModbusDevice::Error error);
    void settings_change();

    void on_read_ready();
private:
    void scheduleReconnect();

    Ui::fmodbus_client *ui;

    QModbusReply *lastRequest = nullptr;
    QModbusClient *modbusDevice = nullptr;

    ModbusConnection _connection_type = ModbusTCP;

    bool _requestInProgress = false;
    QTimer *_reconnectTimer = nullptr;
    int _failedAttempts = 0;
    const int MAX_FAILED_ATTEMPTS = 3;

    QQueue<QModbusDataUnit> _requestQueue;
    bool _processingQueue = false;
    const int MAX_QUEUE_SIZE = 3;
};

#endif // MODBUSCLIENT_H

