#ifndef MODBUSCLIENT_H
#define MODBUSCLIENT_H

#include <QMainWindow>
#include <QModbusDevice>
#include <QQueue>

#include "Config.h"

#include "ModbusBase.h"

#include "qmodbusclient.h"

QT_BEGIN_NAMESPACE

class QModbusClient;
class QModbusReply;

//namespace Ui { class fmodbus_client; }

QT_END_NAMESPACE

class ModbusClient : public QWidget, public ModbusBase {
    Q_OBJECT
public:
    ModbusClient(QWidget *parent = nullptr, QString config = "", ModbusSettings = ModbusSettings());
    ~ModbusClient();

    void set_async(bool async);
    bool isConnected() {return (this->modbusDevice->state() == QModbusDevice::ConnectedState);};
    // bool isReadable() {return this->_socket->isReadable();};
    // int bytesAvailable() {return this->_socket->bytesAvailable();};
    // bool waitForReadyRead(int msecs = 1000) {return this->_socket->waitForReadyRead(msecs);};
    bool reset() {return false;};

    void set_modbus_settings(ModbusSettings modbus_settings);

    bool processingQueue() const {return _processingQueue;};
signals:
    void host_error(QString error);
    void host_connected();
    void host_connecting();
    void host_disconnected();
    ////void read_data(QByteArray rxBuff);

    void read_ready(QModbusDataUnit mdu);
    //void poll_timer_changed(int value);
    void on_connected();
    void on_connecting();
    void on_disconnected();

    void on_state_changed(int state);
public slots:
    //int get_poll_timer() const;
    void on_connect_type(int);
    void on_connect();
    void connectToHost() {this->on_connect();};
    void disconnectFromHost();

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
//    void read_config();
//    void write_config();
private slots:
    void onModbusStateChanged(int state);
    void onModbusErrorOccurred(QModbusDevice::Error error);
    void settings_change();

    void on_read_ready();

    void checkSerialPorts();
private:
    void scheduleReconnect();
    bool isSerialPortAvailable(const QString &portName);

    //Ui::fmodbus_client *ui;

    QModbusReply *lastRequest = nullptr;
    QModbusClient *modbusDevice = nullptr;

    ModbusSettings _modbus_settings;
    ModbusConnection _connection_type = ModbusTCP;

    bool _requestInProgress = false;
    QTimer *_reconnectTimer = nullptr;
    int _failedAttempts = 0;
    const int MAX_FAILED_ATTEMPTS = 3;

    QQueue<QModbusDataUnit> _requestQueue;
    bool _processingQueue = false;
    const int MAX_QUEUE_SIZE = 3;

    QTimer *_portCheckTimer;  // Таймер для проверки портов
    QString _lastKnownPortState;  // Для отслеживания изменений состояния порта
};

#endif // MODBUSCLIENT_H

