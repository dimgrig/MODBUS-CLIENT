#include "ModbusClient.h"
#include "ui_modbus_client.h"

#include <QModbusTcpClient>
#include <QModbusRtuSerialMaster>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QUrl>

#include "Sleeper.h"

#include "functions.h"

ModbusClient::ModbusClient(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::fmodbus_client)
{
    ui->setupUi(this);
    this->on_read_json(this->_mdum);

//    auto sp = this->sizePolicy();
//    sp.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
//    sp.setVerticalPolicy(QSizePolicy::MinimumExpanding);
//    this->setSizePolicy(sp);

    connect(this->ui->le_IP, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    connect(this->ui->le_IP, &QLineEdit::editingFinished, this, &ModbusClient::settings_change);
    connect(this->ui->sb_poll_timer, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value){
        emit poll_timer_changed(value);
    });
}

ModbusClient::~ModbusClient()
{
    if (modbusDevice)
        modbusDevice->disconnectDevice();
    delete modbusDevice;
}

int ModbusClient::get_poll_timer() const {
    return this->ui->sb_poll_timer->value();
}

void ModbusClient::on_connect_type(int index)
{
    if (modbusDevice) {
        modbusDevice->disconnectDevice();
        delete modbusDevice;
        modbusDevice = nullptr;
    }

    auto type = static_cast<ModbusConnection>(index);
    if (type == Serial) {
#if QT_CONFIG(modbus_serialport)
        modbusDevice = new QModbusRtuSerialMaster(this);
#endif
    } else if (type == Tcp) {
        modbusDevice = new QModbusTcpClient(this);
    }

    connect(modbusDevice, &QModbusClient::errorOccurred, [this](QModbusDevice::Error) {
        qCDebug(logDebug()) << modbusDevice->errorString();
        QTimer::singleShot(1000, [=]() {
            this->on_connect();

            _request = false;
        });
        //statusBar()->showMessage(modbusDevice->errorString(), 5000);
    });

    if (!modbusDevice) {
        //ui->connectButton->setDisabled(true);
        if (type == Serial)
            qCDebug(logDebug) << (tr("Could not create Modbus master."), 5000);
        else
            qCDebug(logDebug) << (tr("Could not create Modbus client."), 5000);
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
        if (_connection_type == Serial) {
//            modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
//                ui->portEdit->text());
//#if QT_CONFIG(modbus_serialport)
//            modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter,
//                m_settingsDialog->settings().parity);
//            modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
//                m_settingsDialog->settings().baud);
//            modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
//                m_settingsDialog->settings().dataBits);
//            modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
//                m_settingsDialog->settings().stopBits);
//#endif
        } else {
            const QUrl url = QUrl::fromUserInput(_address);
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, url.port());
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, url.host());
        }
        modbusDevice->setTimeout(2000);
        modbusDevice->setNumberOfRetries(3);
        if (!modbusDevice->connectDevice()) {
            qCDebug(logDebug) << tr("Connect failed: ") + modbusDevice->errorString();
        } else {
            //Sleeper sl;
            //sl.sleep(5);
            qCDebug(logDebug) << tr("Connected: ") + _address << modbusDevice->state();
            //ui->actionConnect->setEnabled(false);
            //ui->actionDisconnect->setEnabled(true);

            this->_reg = this->setupDeviceData(this->_mdum);
        }
    } else {
        modbusDevice->disconnectDevice();
        //ui->actionConnect->setEnabled(true);
        //ui->actionDisconnect->setEnabled(false);
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
    qCDebug(logDebug()) << modbusDevice->state();

    bool connected = (state != QModbusDevice::UnconnectedState);
    //ui->actionConnect->setEnabled(!connected);
    //ui->actionDisconnect->setEnabled(connected);

    if (state == QModbusDevice::UnconnectedState)
        qCDebug(logDebug) << tr("Disconnect: ") + _address << modbusDevice->state();
        //ui->connectButton->setText(tr("Connect"));
    else if (state == QModbusDevice::ConnectedState) {
        qCDebug(logDebug) << tr("Connect: ") + _address << modbusDevice->state();
        //ui->connectButton->setText(tr("Disconnect"));
    }
}

void ModbusClient::onModbusErrorOccurred(QModbusDevice::Error error) {
    qCDebug(logDebug) << error;
}

void ModbusClient::settings_change() {
    this->_address = this->ui->le_IP->text() + ":" + this->ui->le_PORT->text();
    if (this->modbusDevice) {
        this->modbusDevice->disconnectDevice();
    }
}

void ModbusClient::on_read(QModbusDataUnit mdu) {
    if (!modbusDevice)
        return;

    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    if (_request)
        return;

    if (auto *reply = modbusDevice->sendReadRequest(mdu, _server_address)) {
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

    if (auto *reply = modbusDevice->sendWriteRequest(mdu, _server_address)) {
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

void ModbusClient::on_write(QString reg_name, unsigned int value) {
    if (_mdum.count(reg_name) > 0) {
        QVector<quint16> values(2, 0);
        uint_to_usa(value, &values[0]);
        _mdum[reg_name].setValues(values);
        this->on_write(_mdum.at(reg_name));
    }
}

void ModbusClient::on_write(QString reg_name, double value) {
    if (_mdum.count(reg_name) > 0) {
        QVector<quint16> values(4, 0);
        double_to_usa(value, &values[0]);
        _mdum[reg_name].setValues(values);
        this->on_write(_mdum.at(reg_name));
    }
}

void ModbusClient::setRegister(QModbusDataUnit::RegisterType table, const QString &reg_name, double value) {
    if (!modbusDevice)
        return;

    if (_mdum.count(reg_name) > 0) {
        bool ok = true;

        QModbusDataUnit mdu = _mdum[reg_name];
        //int address = _mdum.at(reg_name).startAddress(); // + 1

        switch (table) {
            break;
            case QModbusDataUnit::Coils:
            {
                QVector<quint16> values {value};
                mdu.setValues(values);
                this->on_write(mdu);
                qCDebug(logDebug) << "setRegister" << reg_name << value;
            }
            break;
            case QModbusDataUnit::HoldingRegisters:
            {
                QVector<quint16> values(4, 0);
                double_to_usa(invert_double_order(value), &values[0]);
                mdu.setValues(values);
                this->on_write(mdu);
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
    //qCDebug(logDebug()) << "cfg.MODBUS_IP" << cfg.MODBUS_IP << cfg.MODBUS_PORT;
    this->ui->le_IP->setText(cfg.MODBUS_IP);
    this->ui->le_PORT->setText(QString::number(cfg.MODBUS_PORT));
    this->ui->sb_poll_timer->setValue(cfg.MODBUS_POLL_TIMER);
    this->settings_change();
}

void ModbusClient::write_config(){
    cfg.MODBUS_IP = this->ui->le_IP->text();
    cfg.MODBUS_PORT = this->ui->le_PORT->text().toInt();
    cfg.MODBUS_POLL_TIMER = this->ui->sb_poll_timer->value();

    QSettings config("config.ini", QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    config.setIniCodec("UTF-8"); //Windows-1251
#endif

    config.beginGroup("parameters");
    config.setValue("MODBUS_IP", cfg.MODBUS_IP);
    config.setValue("MODBUS_PORT", cfg.MODBUS_PORT);
    config.setValue("MODBUS_POLL_TIMER", cfg.MODBUS_POLL_TIMER);
    config.endGroup();
}
