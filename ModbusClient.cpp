#include "ModbusClient.h"
//#include "ui_modbus_client.h"

#include <QModbusTcpClient>
#include <QModbusRtuSerialMaster>
#include <QStandardItemModel>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStatusBar>
#include <QUrl>

#include "Sleeper.h"

#include "functions.h"

ModbusClient::ModbusClient(QWidget *parent, QString config, ModbusSettings modbus_settings)
    : QWidget(parent)
    , _modbus_settings(modbus_settings)
//    , ui(new Ui::fmodbus_client)
{
//    ui->setupUi(this);
    this->on_read_json(config);

    // this->ui->cb_connect_type->addItems(ModbusConnection_sl);
    // this->ui->cb_baudrate->addItems(ModbusBaudRate_sl);
    // this->ui->cb_data_bits->addItems(ModbusDataBits_sl);
    // this->ui->cb_stop_bits->addItems(ModbusStopBits_sl);
    // this->ui->cb_parity->addItems(ModbusParity_sl);

    // connect(this->ui->cb_connect_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    // connect(this->ui->le_address, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    // connect(this->ui->le_ip, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    // connect(this->ui->le_port, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    // connect(this->ui->le_com_port, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    // connect(this->ui->cb_baudrate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    // connect(this->ui->cb_data_bits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    // connect(this->ui->cb_stop_bits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    // connect(this->ui->cb_parity, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    // connect(this->ui->sb_poll_timer, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value){
    //     emit poll_timer_changed(value);
    // });

    this->on_connect_type(static_cast<int>(ModbusConnection_from_text(modbus_settings.MODBUS_TYPE.toStdString().c_str())));

    _reconnectTimer = new QTimer(this);
    _reconnectTimer->setSingleShot(true);
    connect(_reconnectTimer, &QTimer::timeout, this, &ModbusClient::on_connect);

    _portCheckTimer = new QTimer(this);
    connect(_portCheckTimer, &QTimer::timeout, this, &ModbusClient::checkSerialPorts);

    if (_connection_type == ModbusSerial) {
        _portCheckTimer->start(1000); // Проверка каждую секунду
    }
}

ModbusClient::~ModbusClient() {
    if (modbusDevice)
        modbusDevice->disconnectDevice();
    delete modbusDevice;
}

// int ModbusClient::get_poll_timer() const {
//     return this->ui->sb_poll_timer->value();
// }

void ModbusClient::set_modbus_settings(ModbusSettings modbus_settings) {
    _modbus_settings = modbus_settings;

    this->on_connect_type(static_cast<int>(ModbusConnection_from_text(modbus_settings.MODBUS_TYPE.toStdString().c_str())));

    if (_connection_type == ModbusSerial) {
        _portCheckTimer->start(1000); // Проверка каждую секунду
    }
};

void ModbusClient::on_connect_type(int index) {
    _connection_type = static_cast<ModbusConnection>(index);

    if (modbusDevice) {
        modbusDevice->disconnectDevice();
        delete modbusDevice;
        modbusDevice = nullptr;
    }

    if (_connection_type == ModbusSerial) {
        modbusDevice = new QModbusRtuSerialMaster(this);
    } else if (_connection_type == ModbusTCP) {
        modbusDevice = new QModbusTcpClient(this);
    }

    // connect(modbusDevice, &QModbusClient::errorOccurred, [this](QModbusDevice::Error) {
    //     qCDebug(logDebug()) << modbusDevice->errorString();
    //     QTimer::singleShot(1000, [=]() {
    //         this->on_connect();
    //     });
    // });

    if (!modbusDevice) {
        if (_connection_type == ModbusSerial) {
            qCDebug(logDebug) << (tr("Could not create Modbus master."));
        } else if (_connection_type == ModbusTCP) {
            qCDebug(logDebug) << (tr("Could not create Modbus client."));
        }
    } else {
        connect(modbusDevice, &QModbusClient::stateChanged, this, &ModbusClient::onModbusStateChanged);
        connect(modbusDevice, &QModbusClient::stateChanged, this, &ModbusClient::on_state_changed);
        connect(modbusDevice, &QModbusClient::errorOccurred, this, &ModbusClient::onModbusErrorOccurred);
    }
}

void ModbusClient::on_connect() {
    if (!modbusDevice)
        return;

    if (!_requestQueue.isEmpty()) {
        qCDebug(logDebug) << "Clearing request queue on reconnect, size:" << _requestQueue.size();
        _requestQueue.clear();
    }
    _processingQueue = false;

    if (_connection_type == ModbusSerial) {
        QString portName = _modbus_settings.MODBUS_SERIAL_PORT;

        // Проверяем наличие порта в системе
        if (!isSerialPortAvailable(portName)) {
            QString errorMsg = tr("Serial port %1 not found in system").arg(portName);

            // Генерируем сигнал об ошибке
            emit host_error(errorMsg);

            // Планируем повторную проверку через некоторое время
            _failedAttempts++;
            scheduleReconnect();
            return;
        }
    }

    if (modbusDevice->state() != QModbusDevice::ConnectedState) {
        if (_connection_type == ModbusSerial) {
            modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter, _modbus_settings.MODBUS_SERIAL_PORT);
            modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, _modbus_settings.MODBUS_BAUDRATE); //QSerialPort::Baud19200
            modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, _modbus_settings.MODBUS_DATA_BITS); //QSerialPort::Data8
            modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, _modbus_settings.MODBUS_STOP_BITS); //QSerialPort::OneStop
            modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter, _modbus_settings.MODBUS_PARITY); //QSerialPort::EvenParity
        } else if (_connection_type == ModbusTCP) {
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, _modbus_settings.MODBUS_IP);
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, _modbus_settings.MODBUS_PORT);
        }

        emit host_connecting();
        emit on_connecting();

        modbusDevice->setTimeout(1000);
        modbusDevice->setNumberOfRetries(0);
        if (!modbusDevice->connectDevice()) {
            qCDebug(logDebug) << tr("Connect failed: ") + modbusDevice->errorString();
        } else {
            if (_connection_type == ModbusSerial) {
                qCDebug(logDebug) << tr("Connected: ") + _modbus_settings.MODBUS_SERIAL_PORT << modbusDevice->state();
            } else if (_connection_type == ModbusTCP) {
                qCDebug(logDebug) << tr("Connected: ") + _modbus_settings.MODBUS_IP << modbusDevice->state();
            }
            this->setupDeviceData();
        }
    } else {
        //modbusDevice->disconnectDevice();
    }
}

void ModbusClient::disconnectFromHost() {
    if (modbusDevice)
        modbusDevice->disconnectDevice();
}

void ModbusClient::onModbusStateChanged(int state) {
    QString state_str;
    switch (modbusDevice->state()) {
        case QModbusDevice::UnconnectedState:
            state_str = "Отключено";
        break;
        case QModbusDevice::ConnectingState:
            state_str = "Подключение";
        break;
        case QModbusDevice::ConnectedState:
            state_str = "Подключено";
        break;
        case QModbusDevice::ClosingState:
            state_str = "Отключение";
        break;
    }

    //this->ui->le_state->setText(state_str);
    //qCDebug(logDebug()) << modbusDevice->state();

    if (state == QModbusDevice::UnconnectedState) {
        emit on_disconnected();
        emit host_disconnected();
        qCDebug(logDebug) << tr("Disconnect: ") << modbusDevice->state();
    } else if (state == QModbusDevice::ConnectedState) {
        emit on_connected();
        emit host_connected();
        if (_connection_type == ModbusSerial) {
            qCDebug(logDebug) << tr("Connected: ") + _modbus_settings.MODBUS_SERIAL_PORT << modbusDevice->state();
        } else if (_connection_type == ModbusTCP) {
            qCDebug(logDebug) << tr("Connected: ") + _modbus_settings.MODBUS_IP << modbusDevice->state();
        }
    }

    if (state == QModbusDevice::ConnectedState) {
        _failedAttempts = 0;
    }
}

void ModbusClient::onModbusErrorOccurred(QModbusDevice::Error error) {
    qCDebug(logDebug) << error << modbusDevice->errorString();

    // Для критических ошибок увеличиваем счетчик
    if (error != QModbusDevice::NoError) {
        _failedAttempts++;
        scheduleReconnect();
    }

    emit host_error(modbusDevice->errorString());
}

void ModbusClient::settings_change() {
    if (this->modbusDevice) {
        this->modbusDevice->disconnectDevice();
        this->on_connect();
    }
}

void ModbusClient::on_read(QModbusDataUnit mdu) {
    if (!modbusDevice)
        return;

    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    if (_processingQueue) {
        if (_requestQueue.size() < MAX_QUEUE_SIZE) {
            _requestQueue.enqueue(mdu);
        }
        return;
    }

    _processingQueue = true;
    sendSingleRequest(mdu);
}

void ModbusClient::sendSingleRequest(QModbusDataUnit mdu) {
    if (auto *reply = modbusDevice->sendReadRequest(mdu, _modbus_settings.MODBUS_ADDRESS)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &ModbusClient::on_read_ready);
        } else {
            _processingQueue = false;
            processNextRequest();
            delete reply;
        }
    } else {
        _processingQueue = false;
        _failedAttempts++;
        scheduleReconnect();
        processNextRequest();
    }
}

void ModbusClient::processNextRequest() {
    if (!_requestQueue.isEmpty()) {
        QModbusDataUnit nextRequest = _requestQueue.dequeue();
        _processingQueue = true;
        sendSingleRequest(nextRequest);
    }
}

void ModbusClient::on_read(QModbusDataUnit::RegisterType type, int startAddress, quint16 numberOfEntries) {
    this->on_read(QModbusDataUnit(type, startAddress, numberOfEntries));
}

void ModbusClient::on_read_ready() {
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) {
        _processingQueue = false;
        processNextRequest();
        return;
    }

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        emit read_ready(unit);
        _failedAttempts = 0;
    } else if (reply->error() == QModbusDevice::ProtocolError) {
        qCDebug(logDebug) << (tr("Read response error: %1 (Mobus exception: 0x%2)").
                                    arg(reply->errorString()).
                                    arg(reply->rawResult().exceptionCode(), -1, 16));
    } else {
        qCDebug(logDebug) << (tr("Read response error: %1 (code: 0x%2)").
                                    arg(reply->errorString()).
                                    arg(reply->error(), -1, 16));
    }

    reply->deleteLater();
    _processingQueue = false;
    processNextRequest();
}

void ModbusClient::on_write(QModbusDataUnit mdu) {
    if (!modbusDevice)
        return;

    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    //statusBar()->clearMessage();

    if (auto *reply = modbusDevice->sendWriteRequest(mdu, _modbus_settings.MODBUS_ADDRESS)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::ProtocolError) {
                    qCDebug(logDebug) << (QString("Write response error: %1 (Mobus exception: 0x%2)")
                        .arg(reply->errorString()).arg(reply->rawResult().exceptionCode(), -1, 16));
                } else if (reply->error() != QModbusDevice::NoError) {
                    qCDebug(logDebug) << (QString("Write response error: %1 (code: 0x%2)").
                        arg(reply->errorString()).arg(reply->error(), -1, 16));
                }
                reply->deleteLater();
            });
        } else {
            // broadcast replies return immediately
            reply->deleteLater();
        }
    } else {
        qCDebug(logDebug) << (tr("Write error: ") + modbusDevice->errorString());
    }
}

void ModbusClient::on_write(QModbusDataUnit::RegisterType type, int startAddress, quint16 numberOfEntries) {
    this->on_write(QModbusDataUnit(type, startAddress, numberOfEntries));
}

//void ModbusClient::on_write(QString reg_name, unsigned int value) {
//    if (_mdum.count(reg_name) > 0) {
//        QVector<quint16> values(2, 0);
//        uint_to_usa(value, &values[0]);
//        _mdum[reg_name].setValues(values);
//        this->on_write(_mdum.at(reg_name));
//    }
//}

//void ModbusClient::on_write(QString reg_name, double value) {
//    if (_mdum.count(reg_name) > 0) {
//        QVector<quint16> values(4, 0);
//        double_to_usa(value, &values[0]);
//        _mdum[reg_name].setValues(values);
//        this->on_write(_mdum.at(reg_name));
//    }
//}

void ModbusClient::setRegister(QModbusDataUnit::RegisterType table, const QString &reg_name, double value) {
    if (!modbusDevice)
        return;

    if (_mdum.count(reg_name) > 0) {
        bool ok = true;

        QModbusDataUnit mdu = _mdum[reg_name];

        switch (table) {
            break;
            case QModbusDataUnit::Coils:
            {
                QVector<quint16> values {(quint16)value};
                mdu.setValues(values);
                this->on_write(mdu);
                //qCDebug(logDebug) << "setRegister" << reg_name << value;
            }
            break;
            case QModbusDataUnit::HoldingRegisters:
            {            
                auto reg_type_m = this->get_reg_type_m();
                QString reg_type = "double";
                if (reg_type_m.count(reg_name) > 0) {
                    reg_type = reg_type_m[reg_name];
                }

                if ((mdu.valueCount() == 4) && (reg_type.toLower().contains("double"))) {
                    QVector<quint16> values(4, 0);
                    double_to_usa(invert_double_order(value), &values[0]);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if ((mdu.valueCount() == 2) && (reg_type.toLower().contains("float"))) {
                    QVector<quint16> values(2, 0);
                    float_to_usa(invert_float_order(value), &values[0]);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if ((mdu.valueCount() == 4) && (reg_type.toLower().contains("ulong"))) {
                    QVector<quint16> values(4, 0);
                    ulong_to_usa(invert_ulong_order(value), &values[0]);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if ((mdu.valueCount() == 4) && (reg_type.toLower().contains("long"))) {
                    QVector<quint16> values(4, 0);
                    long_to_usa(invert_long_order(value), &values[0]);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if ((mdu.valueCount() == 2) && (reg_type.toLower().contains("uint"))) {
                    QVector<quint16> values(2, 0);
                    uint_to_usa(invert_uint_order(value), &values[0]);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if ((mdu.valueCount() == 2) && (reg_type.toLower().contains("int"))) {
                    QVector<quint16> values(2, 0);
                    int_to_usa(invert_int_order(value), &values[0]);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if ((mdu.valueCount() == 1) && (reg_type.toLower().contains("ushort"))) {
                    QVector<quint16> values(1, value);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if ((mdu.valueCount() == 1) && (reg_type.toLower().contains("short"))) {
                    QVector<quint16> values(1, value);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if (mdu.valueCount() == 1) {
                    QVector<quint16> values(1, value);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else {

                }
                //qCDebug(logDebug) << "setRegister" << reg_name << value << mdu.values();
            }
            break;
            default:

            break;
        }
//        if (!ok)
//            statusBar()->showMessage(tr("Could not set register: ") + modbusDevice->errorString());
    }

    // qCDebug(logDebug()) << _mdum.size();
    // for (const auto &value : _mdum) {
    //     qCDebug(logDebug()) << value.first << value.second.startAddress() << value.second.valueCount() << value.second.values();
    // }
}

void ModbusClient::setRegisterString(QModbusDataUnit::RegisterType table, const QString &reg_name, const QString &value) {
    if (!modbusDevice)
        return;

    if (_mdum.count(reg_name) > 0) {
        QModbusDataUnit mdu = _mdum[reg_name];
        int size = mdu.valueCount();
        int maxBytes = size * 2;

        switch (table) {
        case QModbusDataUnit::HoldingRegisters:
        {
            // Преобразуем строку в массив quint16 (каждый регистр = 2 байта)
            QByteArray byteArray = value.toUtf8();

            // Обрезаем строку, если она слишком длинная
            if (byteArray.size() > maxBytes - 1) {
                byteArray.resize(maxBytes - 1);
                qCDebug(logWarning) << "String truncated to fit in registers:" << reg_name;
            }

            byteArray.append('\0');

            int registerCount = (byteArray.size() + 1) / 2;
            QVector<quint16> values(registerCount, 0);

            for (int i = 0; i < byteArray.size(); i++) {
                int registerIndex = i / 2;
                if (i % 2 == 0) {
                    // Старший байт
                    values[registerIndex] = (byteArray[i] << 8) & 0xFF00;
                } else {
                    // Младший байт
                    values[registerIndex] |= (byteArray[i] & 0x00FF);
                }
            }

            // Заполняем нулями
            while (values.size() < size) {
                values.append(0);
            }

            mdu.setValues(values);
            this->on_write(mdu);
            //qCDebug(logDebug) << "setRegisterString" << reg_name << value << "as" << values;
        }
        break;
        default:
            qCDebug(logWarning) << "String write not supported for register type:" << table;
            break;
        }
    }
}

// void ModbusClient::read_config(){
//     //qCDebug(logDebug()) << "cfg.MODBUS_IP" << cfg.MODBUS_TYPE << cfg.MODBUS_IP << cfg.MODBUS_PORT;
//     this->ui->cb_connect_type->setCurrentText(cfg.MODBUS_TYPE);
//     this->ui->le_address->setText(QString::number(cfg.MODBUS_ADDRESS));
//     this->ui->le_ip->setText(cfg.MODBUS_IP);
//     this->ui->le_port->setText(QString::number(cfg.MODBUS_PORT));
//     this->ui->le_com_port->setText(cfg.MODBUS_SERIAL_PORT);
//     this->ui->cb_baudrate->setCurrentText(ModbusBaudRate_to_text(cfg.MODBUS_BAUDRATE));
//     this->ui->cb_data_bits->setCurrentText(ModbusDataBits_to_text(cfg.MODBUS_DATA_BITS));
//     this->ui->cb_stop_bits->setCurrentText(ModbusStopBits_to_text(cfg.MODBUS_STOP_BITS));
//     this->ui->cb_parity->setCurrentText(cfg.MODBUS_PARITY);
//     this->ui->sb_poll_timer->setValue(cfg.MODBUS_POLL_TIMER);

//     this->on_connect_type(this->ui->cb_connect_type->currentIndex());
// }

// void ModbusClient::write_config(){
//     cfg.MODBUS_TYPE = this->ui->cb_connect_type->currentText();
//     cfg.MODBUS_ADDRESS = this->ui->le_address->text().toInt();
//     cfg.MODBUS_IP = this->ui->le_ip->text();
//     cfg.MODBUS_PORT = this->ui->le_port->text().toInt();
//     cfg.MODBUS_SERIAL_PORT = this->ui->le_com_port->text();
//     cfg.MODBUS_BAUDRATE = ModbusBaudRate_from_text(this->ui->cb_baudrate->currentText().toStdString().c_str());
//     cfg.MODBUS_DATA_BITS = ModbusDataBits_from_text(this->ui->cb_data_bits->currentText().toStdString().c_str());
//     cfg.MODBUS_STOP_BITS = ModbusStopBits_from_text(this->ui->cb_stop_bits->currentText().toStdString().c_str());
//     cfg.MODBUS_PARITY = this->ui->cb_parity->currentText();
//     cfg.MODBUS_POLL_TIMER = this->ui->sb_poll_timer->value();

//     QSettings config("config.ini", QSettings::IniFormat);
// #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
//     config.setIniCodec("UTF-8"); //Windows-1251
// #endif

//     config.beginGroup("parameters");
//     config.setValue("MODBUS_TYPE", cfg.MODBUS_TYPE);
//     config.setValue("MODBUS_ADDRESS", cfg.MODBUS_ADDRESS);
//     config.setValue("MODBUS_IP", cfg.MODBUS_IP);
//     config.setValue("MODBUS_PORT", cfg.MODBUS_PORT);
//     config.setValue("MODBUS_SERIAL_PORT", cfg.MODBUS_SERIAL_PORT);
//     config.setValue("MODBUS_BAUDRATE", cfg.MODBUS_BAUDRATE);
//     config.setValue("MODBUS_DATA_BITS", cfg.MODBUS_DATA_BITS);
//     config.setValue("MODBUS_STOP_BITS", cfg.MODBUS_STOP_BITS);
//     config.setValue("MODBUS_PARITY", cfg.MODBUS_PARITY);
//     config.setValue("MODBUS_POLL_TIMER", cfg.MODBUS_POLL_TIMER);
//     config.endGroup();
// }

void ModbusClient::scheduleReconnect() {
    if (_connection_type == ModbusSerial) {
        QString portName = _modbus_settings.MODBUS_SERIAL_PORT;

        if (!isSerialPortAvailable(portName)) {
            // Если порта нет, увеличиваем интервал переподключения
            int delay = 5000; // 5 секунд для Serial, когда порта нет
            qCDebug(logWarning) << "Serial port" << portName << "not available, retrying in" << delay << "ms";
            _reconnectTimer->start(delay);
            return;
        }
    }

    if (_failedAttempts >= MAX_FAILED_ATTEMPTS) {
        qCDebug(logDebug) << "Max failed attempts reached, waiting before reconnect";
        _reconnectTimer->start(5000);
    } else {
        // Исправленный exponential backoff: 1s, 2s, 4s...
        int delay = 1000 * (1 << (_failedAttempts - 1));
        delay = qMin(delay, 5000); // Максимум 5 секунд
        qCDebug(logDebug) << "Scheduling reconnect in" << delay << "ms, attempt" << _failedAttempts;
        _reconnectTimer->start(delay);
    }

    // Очищаем очередь при планировании переподключения
    if (!_requestQueue.isEmpty()) {
        qCDebug(logDebug) << "Clearing queue due to reconnect, had" << _requestQueue.size() << "pending requests";
        _requestQueue.clear();
    }
    _processingQueue = false;
}

bool ModbusClient::isSerialPortAvailable(const QString &portName) {
    if (portName.isEmpty()) {
        return false;
    }

    // Получаем список доступных портов
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    // Ищем нужный порт
    for (const QSerialPortInfo &port : ports) {
        if (port.portName() == portName ||
            port.systemLocation() == portName) {
            qCDebug(logDebug) << "Serial port found:" << port.portName();
            return true;
        }
    }

    // qCDebug(logWarning) << "Serial port not found:" << portName
    //                     << "Available ports:";
    // for (const QSerialPortInfo &port : ports) {
    //     qCDebug(logWarning) << "  -" << port.portName()
    //     << "(" << port.description() << ")";
    // }

    return false;
}

void ModbusClient::checkSerialPorts() {
    if (_connection_type != ModbusSerial) {
        _portCheckTimer->stop();
        return;
    }

    QString portName = _modbus_settings.MODBUS_SERIAL_PORT;
    bool isAvailable = isSerialPortAvailable(portName);

    // Если порт стал доступен и мы не подключены - пытаемся подключиться
    if (isAvailable && modbusDevice->state() != QModbusDevice::ConnectedState) {
        qCDebug(logInfo) << "Serial port" << portName << "became available, attempting to connect";
        on_connect();
    }
    // Если порт исчез и мы подключены - отключаемся
    else if (!isAvailable && modbusDevice->state() == QModbusDevice::ConnectedState) {
        qCDebug(logWarning) << "Serial port" << portName << "disappeared, disconnecting";
        modbusDevice->disconnectDevice();
        emit host_error(tr("Serial port %1 disconnected").arg(portName));
    }
}
