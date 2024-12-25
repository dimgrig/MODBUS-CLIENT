#include "ModbusClient.h"
#include "ui_modbus_client.h"

#include <QModbusTcpClient>
#include <QModbusRtuSerialMaster>
#include <QStandardItemModel>
#include <QSerialPort>
#include <QStatusBar>
#include <QUrl>

#include "Sleeper.h"

#include "functions.h"

ModbusClient::ModbusClient(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::fmodbus_client)
{
    ui->setupUi(this);
    this->on_read_json();

    this->ui->cb_connect_type->addItems(ModbusConnection_sl);
    this->ui->cb_baudrate->addItems(ModbusBaudRate_sl);
    this->ui->cb_data_bits->addItems(ModbusDataBits_sl);
    this->ui->cb_stop_bits->addItems(ModbusStopBits_sl);
    this->ui->cb_parity->addItems(ModbusParity_sl);

    connect(this->ui->cb_connect_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    connect(this->ui->le_address, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    connect(this->ui->le_ip, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    connect(this->ui->le_port, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    connect(this->ui->le_com_port, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    connect(this->ui->cb_baudrate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    connect(this->ui->cb_data_bits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    connect(this->ui->cb_stop_bits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    connect(this->ui->cb_parity, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModbusClient::on_connect_type);
    connect(this->ui->sb_poll_timer, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value){
        emit poll_timer_changed(value);
    });
}

ModbusClient::~ModbusClient() {
    if (modbusDevice)
        modbusDevice->disconnectDevice();
    delete modbusDevice;
}

int ModbusClient::get_poll_timer() const {
    return this->ui->sb_poll_timer->value();
}

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

    connect(modbusDevice, &QModbusClient::errorOccurred, [this](QModbusDevice::Error) {
        qCDebug(logDebug()) << modbusDevice->errorString();
        QTimer::singleShot(1000, [=]() {
            this->on_connect();

            _request = false;
        });
    });

    if (!modbusDevice) {
        if (_connection_type == ModbusSerial) {
            qCDebug(logDebug) << (tr("Could not create Modbus master."), 5000);
        } else if (_connection_type == ModbusTCP) {
            qCDebug(logDebug) << (tr("Could not create Modbus client."), 5000);
        }
    } else {
        connect(modbusDevice, &QModbusClient::stateChanged,
                this, &ModbusClient::onModbusStateChanged);
        connect(modbusDevice, &QModbusClient::errorOccurred,
                this, &ModbusClient::onModbusErrorOccurred);
    }
}

void ModbusClient::on_connect()
{
    if (!modbusDevice)
        return;

    if (modbusDevice->state() != QModbusDevice::ConnectedState) {
        if (_connection_type == ModbusSerial) {
            modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter, this->ui->le_com_port->text());
            modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
                                                 ModbusBaudRate_from_text(this->ui->cb_baudrate->currentText().toStdString().c_str())); //QSerialPort::Baud19200
            modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
                                                 ModbusDataBits_from_text(this->ui->cb_data_bits->currentText().toStdString().c_str())); //QSerialPort::Data8
            modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
                                                 ModbusStopBits_from_text(this->ui->cb_stop_bits->currentText().toStdString().c_str())); //QSerialPort::OneStop
            modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter,
                                                 ModbusParity_from_text(this->ui->cb_parity->currentText().toStdString().c_str())); //QSerialPort::EvenParity
        } else if (_connection_type == ModbusTCP) {
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, this->ui->le_ip->text());
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, this->ui->le_port->text());
        }
        modbusDevice->setTimeout(2000);
        modbusDevice->setNumberOfRetries(3);
        if (!modbusDevice->connectDevice()) {
            qCDebug(logDebug) << tr("Connect failed: ") + modbusDevice->errorString();
        } else {
            if (_connection_type == ModbusSerial) {
                qCDebug(logDebug) << tr("Connected: ") + this->ui->le_com_port->text() << modbusDevice->state();
            } else if (_connection_type == ModbusTCP) {
                qCDebug(logDebug) << tr("Connected: ") + this->ui->le_ip->text() << modbusDevice->state();
            }
            this->setupDeviceData();
        }
    } else {
        modbusDevice->disconnectDevice();
    }
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

    this->ui->le_state->setText(state_str);
    //qCDebug(logDebug()) << modbusDevice->state();

    if (state == QModbusDevice::UnconnectedState)
        qCDebug(logDebug) << tr("Disconnect: ") << modbusDevice->state();
    else if (state == QModbusDevice::ConnectedState) {
        if (_connection_type == ModbusSerial) {
            qCDebug(logDebug) << tr("Connected: ") + this->ui->le_com_port->text() << modbusDevice->state();
        } else if (_connection_type == ModbusTCP) {
            qCDebug(logDebug) << tr("Connected: ") + this->ui->le_ip->text() << modbusDevice->state();
        }
    }
}

void ModbusClient::onModbusErrorOccurred(QModbusDevice::Error error) {
    qCDebug(logDebug) << error;
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

    if (_request)
        return;

    if (auto *reply = modbusDevice->sendReadRequest(mdu, this->ui->le_address->text().toInt())) {
        if (!reply->isFinished())
            connect(reply, &QModbusReply::finished, this, &ModbusClient::on_read_ready);
        else
            delete reply; // broadcast replies return immediately
    } else {
        qCDebug(logDebug) << (tr("Read error: ") + modbusDevice->errorString(), 5000);
    }
}

void ModbusClient::on_read(QModbusDataUnit::RegisterType type, int startAddress, quint16 numberOfEntries) {
    this->on_read(QModbusDataUnit(type, startAddress, numberOfEntries));
}

void ModbusClient::on_read_ready() {
    _request = false;

    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
//        for (int i = 0, total = int(unit.valueCount()); i < total; ++i) {
//            const QString entry = tr("Address: %1, Value: %2").arg(unit.startAddress() + i)
//                                     .arg(QString::number(unit.value(i),
//                                          unit.registerType() <= QModbusDataUnit::Coils ? 10 : 16));
//            //ui->readValue->addItem(entry);
//            qCDebug(logDebug) << entry;
//        }
        emit read_ready(unit);
    } else if (reply->error() == QModbusDevice::ProtocolError) {
        qCDebug(logDebug) << (tr("Read response error: %1 (Mobus exception: 0x%2)").
                                    arg(reply->errorString()).
                                    arg(reply->rawResult().exceptionCode(), -1, 16), 5000);
    } else {
        qCDebug(logDebug) << (tr("Read response error: %1 (code: 0x%2)").
                                    arg(reply->errorString()).
                                    arg(reply->error(), -1, 16), 5000);
    }

    reply->deleteLater();
}

void ModbusClient::on_write(QModbusDataUnit mdu) {
    if (!modbusDevice)
        return;

    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    if (_request)
        return;

    //statusBar()->clearMessage();

    if (auto *reply = modbusDevice->sendWriteRequest(mdu, this->ui->le_address->text().toInt())) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                _request = false;
                if (reply->error() == QModbusDevice::ProtocolError) {
                    qCDebug(logDebug) << (QString("Write response error: %1 (Mobus exception: 0x%2)")
                        .arg(reply->errorString()).arg(reply->rawResult().exceptionCode(), -1, 16),
                        5000);
                } else if (reply->error() != QModbusDevice::NoError) {
                    qCDebug(logDebug) << (QString("Write response error: %1 (code: 0x%2)").
                        arg(reply->errorString()).arg(reply->error(), -1, 16), 5000);
                }
                reply->deleteLater();
            });
        } else {
            _request = false;
            // broadcast replies return immediately
            reply->deleteLater();
        }
    } else {
        qCDebug(logDebug) << (tr("Write error: ") + modbusDevice->errorString(), 5000);
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
                } else if ((mdu.valueCount() == 4) && (reg_type.toLower().contains("long"))) {
                    QVector<quint16> values(4, 0);
                    long_to_usa(invert_long_order(value), &values[0]);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if ((mdu.valueCount() == 2) && (reg_type.toLower().contains("float"))) {
                    QVector<quint16> values(2, 0);
                    float_to_usa(invert_float_order(value), &values[0]);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if ((mdu.valueCount() == 2) && (reg_type.toLower().contains("int"))) {
                    QVector<quint16> values(2, 0);
                    uint_to_usa(invert_uint_order(value), &values[0]);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else if (mdu.valueCount() == 1) {
                    QVector<quint16> values(1, value);
                    mdu.setValues(values);
                    this->on_write(mdu);
                } else {

                }
                //qCDebug(logDebug) << "setRegister" << reg_name << value << values;
            }
            break;
            default:

            break;
        }
//        if (!ok)
//            statusBar()->showMessage(tr("Could not set register: ") + modbusDevice->errorString(),
//                                     5000);
    }

    // qCDebug(logDebug()) << _mdum.size();
    // for (const auto &value : _mdum) {
    //     qCDebug(logDebug()) << value.first << value.second.startAddress() << value.second.valueCount() << value.second.values();
    // }
}

void ModbusClient::read_config(){
    //qCDebug(logDebug()) << "cfg.MODBUS_IP" << cfg.MODBUS_TYPE << cfg.MODBUS_IP << cfg.MODBUS_PORT;
    this->ui->cb_connect_type->setCurrentText(cfg.MODBUS_TYPE);
    this->ui->le_address->setText(QString::number(cfg.MODBUS_ADDRESS));
    this->ui->le_ip->setText(cfg.MODBUS_IP);
    this->ui->le_port->setText(QString::number(cfg.MODBUS_PORT));
    this->ui->le_com_port->setText(cfg.MODBUS_SERIAL_PORT);
    this->ui->cb_baudrate->setCurrentText(ModbusBaudRate_to_text(cfg.MODBUS_BAUDRATE));
    this->ui->cb_data_bits->setCurrentText(ModbusDataBits_to_text(cfg.MODBUS_DATA_BITS));
    this->ui->cb_stop_bits->setCurrentText(ModbusStopBits_to_text(cfg.MODBUS_STOP_BITS));
    this->ui->cb_parity->setCurrentText(cfg.MODBUS_PARITY);
    this->ui->sb_poll_timer->setValue(cfg.MODBUS_POLL_TIMER);

    this->on_connect_type(this->ui->cb_connect_type->currentIndex());
}

void ModbusClient::write_config(){
    cfg.MODBUS_TYPE = this->ui->cb_connect_type->currentText();
    cfg.MODBUS_ADDRESS = this->ui->le_address->text().toInt();
    cfg.MODBUS_IP = this->ui->le_ip->text();
    cfg.MODBUS_PORT = this->ui->le_port->text().toInt();
    cfg.MODBUS_SERIAL_PORT = this->ui->le_com_port->text();
    cfg.MODBUS_BAUDRATE = ModbusBaudRate_from_text(this->ui->cb_baudrate->currentText().toStdString().c_str());
    cfg.MODBUS_DATA_BITS = ModbusDataBits_from_text(this->ui->cb_data_bits->currentText().toStdString().c_str());
    cfg.MODBUS_STOP_BITS = ModbusStopBits_from_text(this->ui->cb_stop_bits->currentText().toStdString().c_str());
    cfg.MODBUS_PARITY = this->ui->cb_parity->currentText();
    cfg.MODBUS_POLL_TIMER = this->ui->sb_poll_timer->value();

    QSettings config("config.ini", QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    config.setIniCodec("UTF-8"); //Windows-1251
#endif

    config.beginGroup("parameters");
    config.setValue("MODBUS_TYPE", cfg.MODBUS_TYPE);
    config.setValue("MODBUS_ADDRESS", cfg.MODBUS_ADDRESS);
    config.setValue("MODBUS_IP", cfg.MODBUS_IP);
    config.setValue("MODBUS_PORT", cfg.MODBUS_PORT);
    config.setValue("MODBUS_SERIAL_PORT", cfg.MODBUS_SERIAL_PORT);
    config.setValue("MODBUS_BAUDRATE", cfg.MODBUS_BAUDRATE);
    config.setValue("MODBUS_DATA_BITS", cfg.MODBUS_DATA_BITS);
    config.setValue("MODBUS_STOP_BITS", cfg.MODBUS_STOP_BITS);
    config.setValue("MODBUS_PARITY", cfg.MODBUS_PARITY);
    config.setValue("MODBUS_POLL_TIMER", cfg.MODBUS_POLL_TIMER);
    config.endGroup();
}
