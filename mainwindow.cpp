#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QNetworkProxy>
#include <QScrollArea>

#include <float.h>
#include <locale.h>

const int StatusTimeout = AQP::MSecPerSecond * 10;

//#define PROCESSEVENTS
#define SINGLESHOT

#define SINGLESHOT_TIMER    100

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //qCDebug(logDebug()) << this->ui->dw_batch_parameters->height();

    TestRunner tr;
    RUN_TEST(tr, FunctionsTest);
    RUN_TEST(tr, MacrosesTest);

    this->setFont(QFont("Noto Sans", 10)); //10
    //qCDebug(logDebug()) << this->font();

//    QLocale locale = QLocale::system(); // current locale
//    QLocale russian(QLocale::Russian);
//    locale.setNumberOptions(russian.numberOptions()); // borrow number options from the Russian locale
//    QLocale::setDefault(locale); // set as default
    QLocale::setDefault(QLocale::Russian);
    QLocale::setDefault(QLocale::Russian);
    QLocale::setDefault(QLocale::Russian);
    QLocale locale = QLocale::system(); // current locale
    //if (locale.name() != "ru_RU") {
//        qCDebug(logDebug()) << std::locale().name() << locale.name() << QLocale().name();
//        AQP::warning(this, "Ошибка", "Ошибка установки локали. Выполнить sudo dpkg-reconfigure locales и выбрать локаль ru_RU");
//        exit(1);
//    }

//    qCDebug(logDebug()) << std::locale().name() << locale.name();
//    setlocale(LC_ALL, "ru_RU");
//    //std::locale::global(std::locale(std::locale("ru_RU"), "C", std::locale::numeric));
    //qCDebug(logDebug()) << std::locale().name() << locale.name();

    this->setAttribute(Qt::WA_AlwaysShowToolTips);

    if (QFile(QSS).exists()) {
        QScopedPointer<QFile> styleFile(new QFile(QSS));
        styleFile.data()->open(QFile::ReadOnly);
        QString qssStr = styleFile.data()->readAll();
        styleFile.data()->close();
        this->setStyleSheet(qssStr);
    } else {
        qCDebug(logWarning()) << "style file does not exist";
    }

    //this->ui->centralwidget->hide();

    //this->ui->pb_test->setVisible(false);
    //this->ui->pb_test_oscillogram->setVisible(false);
    //this->ui->pb_quit->setVisible(false);

    //setWindowTitle(QString("%1[*]").arg(QApplication::applicationDisplayName()));
    setWindowTitle(QString("[*]"));

    //this->about_panel->hide();

    //this->installEventFilter(this);

    this->docks();
    this->mvc();

    this->menu_bar();
    this->tool_bar();

    this->connect_signal_slot();

    this->read_config();

    //this->ui->gb_system_settings->layout()->addWidget(this->modbus_client);
    this->ui->modbus_settings_layout->addWidget(this->modbus_client_settings, 3, 0, 2, 0);

    // if (cfg.PROXY) {
    //     QNetworkProxy proxy;
    //     proxy.setType(QNetworkProxy::DefaultProxy);
    //     proxy.setHostName(cfg.PROXY_HOST); //"192.168.7.1"
    //     proxy.setPort(cfg.PROXY_PORT); //3128
    //     //proxy.setUser("username");
    //     //proxy.setPassword("password");

    //     QNetworkProxy::setApplicationProxy(proxy);
    // }


    modbus_client->set_modbus_settings(this->modbus_client_settings->getSettingsFromUi());
    modbus_client->connectToHost();

    this->modbus_ui();

//     this->ui->cb_language->addItems(QStringList() << "ru_RU" << "en_US");
//     this->ui->cb_language->setCurrentText("en_US");

// #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
//     connect(this->ui->cb_language, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
//            [=](const QString &str){
//        qtLanguageTranslator.load("QtLanguage_" + str, QCoreApplication::applicationDirPath());   // Загружаем перевод
//        qApp->installTranslator(&qtLanguageTranslator);        // Устанавливаем перевод в приложение
//     });
// #endif
// #if QT_VERSION > QT_VERSION_CHECK(5, 15, 8)
//     connect(this->ui->cb_language, &QComboBox::currentTextChanged,
//            [=](const QString &str){
//        qtLanguageTranslator.load("QtLanguage_" + str, QCoreApplication::applicationDirPath());   // Загружаем перевод
//        qApp->installTranslator(&qtLanguageTranslator);        // Устанавливаем перевод в приложение
//     });
// #endif

//     qtLanguageTranslator.load(QString("QtLanguage_") + QString("en_US"), QCoreApplication::applicationDirPath());
//     qApp->installTranslator(&qtLanguageTranslator);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (okToClearData())
        event->accept();
    else
        event->ignore();

    this->_saveGeometry();

    this->write_config();

    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *target, QEvent *event) {
    Q_UNUSED(target);
    Q_UNUSED(event);
//    if (event->type() == QEvent::Shortcut) {
//        // If I care about this shortcut, then return true to intercept
//        // Else, return false to let the application process it
//        if (!cfg.ENABLE_SHORTCUTS) {
//            qCDebug(logDebug()) << "SHORTCUT IGNORES";
//            return true;
//        }
//        qCDebug(logDebug()) << "SHORTCUT RUNS";
//    }

    return false;
}

void MainWindow::connect_signal_slot() {
    connect(this->timer, &QTimer::timeout, this, &MainWindow::on_timer);
    this->timer->start(this->modbus_client_settings->get_poll_timer());

    connect(this->modbus_client, &ModbusClient::read_ready, this, &MainWindow::modbus_read);

    connect(this->modbus_client_settings, &ModbusClientSettings::settings_changed, [this](){
        modbus_client->disconnectFromHost();
        modbus_client->set_modbus_settings(this->modbus_client_settings->getSettingsFromUi());
        modbus_client->connectToHost();
    });

    connect(this->modbus_client_settings, &ModbusClientSettings::poll_timer_changed, [this](int value){
        this->timer->stop();
        this->timer->start(value);
    });

    connect(this->modbus_client, &ModbusClient::on_state_changed, this->modbus_client_settings, &ModbusClientSettings::onModbusStateChanged);
}

void MainWindow::read_config(){


    //this->ui->sb_poll_timer->setValue(cfg.MODBUS_POLL_TIMER);

    this->modbus_client_settings->read_config();
}

void MainWindow::write_config(){
    //cfg.MODBUS_POLL_TIMER = this->ui->sb_poll_timer->value();

    QSettings config("config.ini", QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    config.setIniCodec("UTF-8"); //Windows-1251
#endif

    config.beginGroup("parameters");
    //config.setValue("MODBUS_POLL_TIMER", cfg.MODBUS_POLL_TIMER);
    config.endGroup();

    this->modbus_client_settings->write_config();
}

void MainWindow::menu_bar() {

}

void MainWindow::tool_bar() {

}

void MainWindow::auth_users() {

}

void MainWindow::init_auth_users() {

}

void MainWindow::mvc() {

}

void MainWindow::docks() {

}

void MainWindow::_saveGeometry() {

}

void MainWindow::test() {

}

bool MainWindow::okToClearData()
{
//    if (isWindowModified())
//        return AQP::okToClearData(&MainWindow::save_as, this, tr("Несохраненные изменения"), tr("Тестовая карта содержит несохраненные изменения. Сохранить изменения?"));
    return true;
}

void MainWindow::auth_ok() {

}

void MainWindow::on_timer() {
    //qCDebug(logDebug()) << "modbus_read_request";
    auto reg_v = modbus_client->get_reg_v();
    for (const auto &reg : reg_v) {
        if (reg.contains(QModbusDataUnit::RegisterType::DiscreteInputs)) {
            this->modbus_client->on_read(QModbusDataUnit::RegisterType::DiscreteInputs,
                                         reg[QModbusDataUnit::RegisterType::DiscreteInputs].startAddress(),
                                         reg[QModbusDataUnit::RegisterType::DiscreteInputs].valueCount());
        }
        if (reg.contains(QModbusDataUnit::RegisterType::Coils)) {
            this->modbus_client->on_read(QModbusDataUnit::RegisterType::Coils,
                                         reg[QModbusDataUnit::RegisterType::Coils].startAddress(),
                                         reg[QModbusDataUnit::RegisterType::Coils].valueCount());
        }
        if (reg.contains(QModbusDataUnit::RegisterType::InputRegisters)) {
            this->modbus_client->on_read(QModbusDataUnit::RegisterType::InputRegisters,
                                         reg[QModbusDataUnit::RegisterType::InputRegisters].startAddress(),
                                         reg[QModbusDataUnit::RegisterType::InputRegisters].valueCount());
        }
        if (reg.contains(QModbusDataUnit::RegisterType::HoldingRegisters)) {
            this->modbus_client->on_read(QModbusDataUnit::RegisterType::HoldingRegisters,
                                         reg[QModbusDataUnit::RegisterType::HoldingRegisters].startAddress(),
                                         reg[QModbusDataUnit::RegisterType::HoldingRegisters].valueCount());
        }
    }
}

void MainWindow::modbus_read(QModbusDataUnit mdu) {
    qCDebug(logDebug()) << "modbus_read" << "registerType" << mdu.registerType() << "valueCount" << mdu.values().size() << "values" << mdu.values();

    switch (mdu.registerType()) {
        case QModbusDataUnit::DiscreteInputs:
        {
            for (int i = mdu.startAddress(), j = 0; i < mdu.startAddress() + (int)mdu.valueCount(); ++i, ++j) {
                for (const auto &_mdu : _mdum) {
                    if ((_mdu.second.registerType() == mdu.registerType()) &&
                        (_mdu.second.startAddress() == (mdu.startAddress() + j))) {
                        auto cb = this->ui->gb1->findChild<QCheckBox*>("cb_" + _mdu.first);
                        if (cb) {
                            cb->setChecked(mdu.value(j));
                        }
                        break;
                    }
                }
            }
        }
        break;
        case QModbusDataUnit::Coils:
        {
            for (int i = mdu.startAddress(), j = 0; i < mdu.startAddress() + (int)mdu.valueCount(); ++i, ++j) {
                for (const auto &_mdu : _mdum) {
                    if ((_mdu.second.registerType() == mdu.registerType()) &&
                        (_mdu.second.startAddress() == (mdu.startAddress() + j))) {
                        auto cb = this->ui->gb2->findChild<QCheckBox*>("cb_" + _mdu.first);
                        if (cb) {
                            disconnect(cb, &QCheckBox::clicked, this, &MainWindow::CoilsChanged);
                            cb->setChecked(mdu.value(j));
                            connect(cb, &QCheckBox::clicked, this, &MainWindow::CoilsChanged);
                        }
                        break;
                    }
                }
            }
        }
        break;
        case QModbusDataUnit::InputRegisters:
        {
            for (int i = mdu.startAddress(), j = 0; i < mdu.startAddress() + (int)mdu.valueCount(); ) { //i = i + 4, j = j + 4
                bool found = false;
                for (const auto &_mdu : _mdum) {
                    if ((_mdu.second.registerType() == mdu.registerType()) &&
                        (_mdu.second.startAddress() == (mdu.startAddress() + j))) {

                        auto reg_type_m = modbus_client->get_reg_type_m();
                        QString reg_type = "double";
                        if (reg_type_m.count(_mdu.first) > 0) {
                            reg_type = reg_type_m[_mdu.first];
                        }

                        if (reg_type.toLower().contains("string")) {
                            auto le = this->ui->gb3->findChild<QLineEdit*>("le_" + _mdu.first);
                            if (le) {
                                // Преобразуем регистры обратно в строку
                                QByteArray byteArray;
                                for (int k = 0; k < _mdu.second.valueCount(); k++) {
                                    if (j + k < mdu.values().size()) {
                                        quint16 reg = mdu.values()[j + k];
                                        byteArray.append((reg >> 8) & 0xFF); // Старший байт
                                        byteArray.append(reg & 0xFF);        // Младший байт
                                    }
                                }
                                // Обрезаем до нулевого байта или непечатных символов
                                int nullPos = byteArray.indexOf('\0');
                                if (nullPos != -1) {
                                    byteArray.truncate(nullPos);
                                } else {
                                    int actualLength = byteArray.size();
                                    for (int i = 0; i < byteArray.size(); i++) {
                                        char c = byteArray[i];
                                        if (c < 32 || c > 126) {
                                            actualLength = i;
                                            break;
                                        }
                                    }
                                    byteArray.truncate(actualLength);
                                }

                                QString str = QString::fromUtf8(byteArray).trimmed();

                                le->setText(str);
                            }
                            i += _mdu.second.valueCount();
                            j += _mdu.second.valueCount();
                            found = true;
                        } else {
                            auto dsb = this->ui->gb3->findChild<QDoubleSpinBox*>("dsb_" + _mdu.first);
                            //qCDebug(logDebug()) << _mdu.first << mdu.values()[j];
                            if (dsb) {
                                if ((_mdu.second.valueCount() == 4) && (reg_type.toLower().contains("double"))) {
                                    double v = usa_to_double(&mdu.values()[j]);
                                    dsb->setValue(invert_double_order(v));
                                } else if ((_mdu.second.valueCount() == 4) && (reg_type.toLower().contains("ulong"))) {
                                    unsigned long long v = usa_to_ulong(&mdu.values()[j]);
                                    dsb->setValue(invert_ulong_order(v));
                                } else if ((_mdu.second.valueCount() == 4) && (reg_type.toLower().contains("long"))) {
                                    long long v = usa_to_long(&mdu.values()[j]);
                                    dsb->setValue(invert_long_order(v));
                                } else if ((_mdu.second.valueCount() == 2) && (reg_type.toLower().contains("float"))) {
                                    float v = usa_to_float(&mdu.values()[j]);
                                    dsb->setValue(invert_float_order(v));
                                } else if ((_mdu.second.valueCount() == 2) && (reg_type.toLower().contains("uint"))) {
                                    unsigned int v = usa_to_uint(&mdu.values()[j]);
                                    dsb->setValue(invert_uint_order(v));
                                } else if ((_mdu.second.valueCount() == 2) && (reg_type.toLower().contains("int"))) {
                                    unsigned int v = usa_to_int(&mdu.values()[j]);
                                    dsb->setValue(invert_int_order(v));
                                } else if ((_mdu.second.valueCount() == 1) && (reg_type.toLower().contains("ushort"))) {
                                    dsb->setValue(mdu.values()[j]);
                                } else if ((_mdu.second.valueCount() == 1) && (reg_type.toLower().contains("short"))) {
                                    dsb->setValue((short)mdu.values()[j]);
                                } else if (_mdu.second.valueCount() == 1) {
                                    dsb->setValue(mdu.values()[j]);
                                } else {

                                }
                                i += _mdu.second.valueCount();
                                j += _mdu.second.valueCount();
                                found = true;
                            }
                        }
                        break;
                    }
                }
                if (!found) {
                    i += 2;
                    j += 2;
                }
            }
        }
        break;
        case QModbusDataUnit::HoldingRegisters:
        {
            for (int i = mdu.startAddress(), j = 0; i < mdu.startAddress() + (int)mdu.valueCount(); ) { //i = i + 4, j = j + 4
                bool found = false;
                for (const auto &_mdu : _mdum) {
                    if ((_mdu.second.registerType() == mdu.registerType()) &&
                        (_mdu.second.startAddress() == (mdu.startAddress() + j))) {

                        auto reg_type_m = modbus_client->get_reg_type_m();
                        QString reg_type = "double";
                        if (reg_type_m.count(_mdu.first) > 0) {
                            reg_type = reg_type_m[_mdu.first];
                        }

                        if (reg_type.toLower().contains("string")) {
                            auto le = this->ui->gb4->findChild<QLineEdit*>("le_" + _mdu.first);
                            if (le) {
                                // Преобразуем регистры обратно в строку
                                QByteArray byteArray;
                                for (int k = 0; k < _mdu.second.valueCount(); k++) {
                                    if (j + k < mdu.values().size()) {
                                        quint16 reg = mdu.values()[j + k];
                                        byteArray.append((reg >> 8) & 0xFF); // Старший байт
                                        byteArray.append(reg & 0xFF);        // Младший байт
                                    }
                                }
                                // Обрезаем до нулевого байта или непечатных символов
                                int nullPos = byteArray.indexOf('\0');
                                if (nullPos != -1) {
                                    byteArray.truncate(nullPos);
                                } else {
                                    int actualLength = byteArray.size();
                                    for (int i = 0; i < byteArray.size(); i++) {
                                        char c = byteArray[i];
                                        if (c < 32 || c > 126) {
                                            actualLength = i;
                                            break;
                                        }
                                    }
                                    byteArray.truncate(actualLength);
                                }

                                QString str = QString::fromUtf8(byteArray).trimmed();

                                disconnect(le, &QLineEdit::textChanged, this, &MainWindow::StringRegistersChanged);
                                le->setText(str);
                                connect(le, &QLineEdit::textChanged, this, &MainWindow::StringRegistersChanged);
                            }
                            i += _mdu.second.valueCount();
                            j += _mdu.second.valueCount();
                            found = true;
                        } else {
                            auto dsb = this->ui->gb4->findChild<QDoubleSpinBox*>("dsb_" + _mdu.first);
                            //qCDebug(logDebug()) << _mdu.first << mdu.values()[j];
                            if (dsb) {
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                                disconnect(dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::HoldingRegistersChanged);
    #endif
    #if QT_VERSION > QT_VERSION_CHECK(5, 15, 8)
                                disconnect(dsb, &QDoubleSpinBox::valueChanged, this, &MainWindow::HoldingRegistersChanged);
    #endif

                                if ((_mdu.second.valueCount() == 4) && (reg_type.toLower().contains("double"))) {
                                    double v = usa_to_double(&mdu.values()[j]);
                                    dsb->setValue(invert_double_order(v));
                                } else if ((_mdu.second.valueCount() == 4) && (reg_type.toLower().contains("ulong"))) {
                                    unsigned long long v = usa_to_ulong(&mdu.values()[j]);
                                    dsb->setValue(invert_ulong_order(v));
                                } else if ((_mdu.second.valueCount() == 4) && (reg_type.toLower().contains("long"))) {
                                    long long v = usa_to_long(&mdu.values()[j]);
                                    dsb->setValue(invert_long_order(v));
                                } else if ((_mdu.second.valueCount() == 2) && (reg_type.toLower().contains("float"))) {
                                    float v = usa_to_float(&mdu.values()[j]);
                                    dsb->setValue(invert_float_order(v));
                                } else if ((_mdu.second.valueCount() == 2) && (reg_type.toLower().contains("uint"))) {
                                    unsigned int v = usa_to_uint(&mdu.values()[j]);
                                    dsb->setValue(invert_uint_order(v));
                                } else if ((_mdu.second.valueCount() == 2) && (reg_type.toLower().contains("int"))) {
                                    int v = usa_to_int(&mdu.values()[j]);
                                    dsb->setValue(invert_int_order(v));
                                } else if ((_mdu.second.valueCount() == 1) && (reg_type.toLower().contains("ushort"))) {
                                    dsb->setValue(mdu.values()[j]);
                                } else if ((_mdu.second.valueCount() == 1) && (reg_type.toLower().contains("short"))) {
                                    dsb->setValue((short)mdu.values()[j]);
                                } else if (_mdu.second.valueCount() == 1) {
                                    dsb->setValue(mdu.values()[j]);
                                } else {

                                }
                                i += _mdu.second.valueCount();
                                j += _mdu.second.valueCount();
                                found = true;


    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                                connect(dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::HoldingRegistersChanged);
    #endif
    #if QT_VERSION > QT_VERSION_CHECK(5, 15, 8)
                                connect(dsb, &QDoubleSpinBox::valueChanged, this, &MainWindow::HoldingRegistersChanged);
    #endif
                            }
                        }
                        break;
                    }
                }
                if (!found) {
                    i += 2;
                    j += 2;
                }
            }
        }
        break;
        default:

        break;
    }
}

void MainWindow::CoilsChanged(bool value) {
    auto sender = QObject::sender();
    if (sender) {
        QString reg_name = sender->objectName();
        reg_name = reg_name.remove("cb_");
        qCDebug(logDebug()) << reg_name << value;
        this->modbus_client->setRegister(QModbusDataUnit::Coils, reg_name, value);
    }
}

void MainWindow::HoldingRegistersChanged(double value) {
    auto sender = QObject::sender();
    if (sender) {
        QString reg_name = sender->objectName();
        reg_name = reg_name.remove("dsb_");
        qCDebug(logDebug()) << reg_name << value;
        this->modbus_client->setRegister(QModbusDataUnit::HoldingRegisters, reg_name, value);
    }
}

void MainWindow::StringRegistersChanged(const QString &value) {
    auto sender = QObject::sender();
    if (sender) {
        QString reg_name = sender->objectName();
        reg_name = reg_name.remove("le_");
        qCDebug(logDebug()) << reg_name << value;
        this->modbus_client->setRegisterString(QModbusDataUnit::HoldingRegisters, reg_name, value);
    }
}

void create_gb(QGridLayout *gl, QGroupBox *gb) {
    QGroupBox* gbi = new QGroupBox;
    //QGridLayout *li = new QGridLayout; //gl
    //QGroupBox *gbo = new QGroupBox; //gb
    QVBoxLayout *lo = new QVBoxLayout;
    QScrollArea* sa = new QScrollArea();

    gbi->setLayout(gl);
    sa->setWidget(gbi);
    sa->setWidgetResizable(true);
    lo->addWidget(sa);
    gb->setLayout(lo);

    sa->setWidgetResizable(true);
    sa->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void create_dsb(QDoubleSpinBox* dsb, const QString &name, const QString &reg_type, const QModbusDataUnit &mdu) {
    Q_UNUSED(mdu);
    dsb->setObjectName(QString("dsb_") + name);
    dsb->setMinimumSize({50, 20});
    if (reg_type.toLower().contains("float") || reg_type.toLower().contains("double")) {
        //float or double
        dsb->setDecimals(2);
        dsb->setSingleStep(0.01);
        dsb->setMaximum(DBL_MAX);
        dsb->setMinimum(-DBL_MAX);
    } else if (reg_type.toLower().contains("ushort")) {
        //ushort
        dsb->setDecimals(0);
        dsb->setSingleStep(1);
        dsb->setMaximum((double)USHRT_MAX);
        dsb->setMinimum(0);
    } else if (reg_type.toLower().contains("short")) {
        //short
        dsb->setDecimals(0);
        dsb->setSingleStep(1);
        dsb->setMaximum((double)SHRT_MAX);
        dsb->setMinimum((double)-SHRT_MAX);
    } else if (reg_type.toLower().contains("uint")) {
        //uint
        dsb->setDecimals(0);
        dsb->setSingleStep(1);
        dsb->setMaximum((double)UINT_MAX);
        dsb->setMinimum(0);
    } else if (reg_type.toLower().contains("int")) {
        //int
        dsb->setDecimals(0);
        dsb->setSingleStep(1);
        dsb->setMaximum((double)INT_MAX);
        dsb->setMinimum((double)-INT_MAX);
    } else if (reg_type.toLower().contains("ulong")) {
        //ulong
        dsb->setDecimals(0);
        dsb->setSingleStep(1);
        dsb->setMaximum((double)ULONG_MAX);
        dsb->setMinimum(0);
    } else if (reg_type.toLower().contains("long")) {
        //long
        dsb->setDecimals(0);
        dsb->setSingleStep(1);
        dsb->setMaximum((double)LONG_MAX);
        dsb->setMinimum((double)-LONG_MAX);
    } else {
        //TODO
    }
}

void MainWindow::modbus_ui() {
    QGridLayout *gl1 = new QGridLayout(this);
    QGridLayout *gl2 = new QGridLayout(this);
    QGridLayout *gl3 = new QGridLayout(this);
    QGridLayout *gl4 = new QGridLayout(this);
    create_gb(gl1, this->ui->gb1);
    create_gb(gl2, this->ui->gb2);
    create_gb(gl3, this->ui->gb3);
    create_gb(gl4, this->ui->gb4);

    this->_mdum = modbus_client->get_mdum();
    auto mdum_v = modbus_client->get_sort_by_start_address_mdum_v();
    auto reg_type_m = modbus_client->get_reg_type_m();

    for (const auto &_mdu : mdum_v) {
        QString name = _mdu.first;
        auto mdu = _mdu.second;

        QString reg_type = "double";
        if (reg_type_m.count(name) > 0) {
            reg_type = reg_type_m[name];
        }
        //qCDebug(logDebug) << name << mdu.startAddress() << mdu.valueCount() << mdu.values();
        switch (mdu.registerType()) {
            case QModbusDataUnit::DiscreteInputs:
            {
                auto label = new QLabel(name, this);
                auto cb = new QCheckBox(this);
                cb->setObjectName(QString("cb_") + name);

                gl1->addWidget(label, di_v.size(), 0);
                gl1->addWidget(cb, di_v.size(), 1);

                di_v.push_back(cb);
            }
            break;
            case QModbusDataUnit::Coils:
            {
                auto label = new QLabel(name, this);
                auto cb = new QCheckBox(this);
                cb->setObjectName(QString("cb_") + name);

                gl2->addWidget(label, coil_v.size(), 0);
                gl2->addWidget(cb, coil_v.size(), 1);

                coil_v.push_back(cb);
                connect(cb, &QCheckBox::clicked, this, &MainWindow::CoilsChanged);
            }
            break;
            case QModbusDataUnit::InputRegisters:
            {
                if (reg_type.toLower().contains("string")) {
                    auto label = new QLabel(name, this);
                    auto le = new QLineEdit(this);
                    le->setObjectName(QString("le_") + name);

                    gl3->addWidget(label, ir_le_v.size() + ir_v.size(), 0);
                    gl3->addWidget(le, ir_le_v.size() + ir_v.size(), 1);

                    ir_le_v.push_back(le);
                } else {
                    auto label = new QLabel(name, this);
                    auto dsb = new QDoubleSpinBox(this);
                    create_dsb(dsb, name, reg_type, mdu);

                    gl3->addWidget(label, ir_le_v.size() + ir_v.size(), 0);
                    gl3->addWidget(dsb, ir_le_v.size() + ir_v.size(), 1);

                    ir_v.push_back(dsb);
                }
            }
            break;
            case QModbusDataUnit::HoldingRegisters:
            {
                if (reg_type.toLower().contains("string")) {
                    auto label = new QLabel(name, this);
                    auto le = new QLineEdit(this);
                    le->setObjectName(QString("le_") + name);

                    gl4->addWidget(label, hr_le_v.size() + hr_v.size(), 0);
                    gl4->addWidget(le, hr_le_v.size() + hr_v.size(), 1);

                    hr_le_v.push_back(le);

                    connect(le, &QLineEdit::textChanged, this, &MainWindow::StringRegistersChanged);
                } else {
                    auto label = new QLabel(name, this);
                    auto dsb = new QDoubleSpinBox(this);
                    create_dsb(dsb, name, reg_type, mdu);

                    gl4->addWidget(label, hr_le_v.size() + hr_v.size(), 0);
                    gl4->addWidget(dsb, hr_le_v.size() + hr_v.size(), 1);

                    hr_v.push_back(dsb);
                    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                        connect(dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::HoldingRegistersChanged);
                    #endif
                    #if QT_VERSION > QT_VERSION_CHECK(5, 15, 8)
                        connect(dsb, &QDoubleSpinBox::valueChanged, this, &MainWindow::HoldingRegistersChanged);
                    #endif
                }
            }
            break;
            default:

            break;
        }
    }
}
