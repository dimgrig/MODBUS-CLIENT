#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableView>


#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>

#include <iostream>

#include <core-widgets-lib/aqp.h>
#include <core-widgets-lib/icons.h>
#include <core-lib/Logger.h>
#include <core-lib/macroses.h>
#include <core-lib/templates.h>
#include <core-lib/Sleeper.h>
#include <core-lib/TestRunner.h>

#include <QGridLayout>
#include <QTranslator>
#include <QLibraryInfo>

#include <core-lib/Profiler.h>

#include <core-modbus-lib/ModbusClient.h>

//using namespace std;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define MODBUS_READ_START_INDEX         0

#define MODBUS_MS_SLEEP 10

class MainWindow : public QMainWindow {
Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void init_auth_users();
signals:
    void return_pressed();
protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *target, QEvent *event) override;
public slots:

private slots:
    void setDirty() { setWindowModified(true); }

    void auth_ok();
    //void on_timer();
    void modbus_read(int serverAddress, QModbusDataUnit mdu);

    void CoilsChanged(bool value);
    void HoldingRegistersChanged(double value);
    void StringRegistersChanged(const QString &value);
private:
    void connect_signal_slot();
    void read_config();
    void write_config();
    void menu_bar();
    void tool_bar();
    void mvc();
    void docks();
    void auth_users();
    void _saveGeometry();
    void restore_geometry();
    void modbus_ui();
    void clear_modbus_ui();

    void test();

    bool okToClearData();

    Ui::MainWindow *ui;

    //QTimer *timer = new QTimer(this);

    ModbusClient *modbus_client = new ModbusClient(this);
    ModbusSettingsUI *modbus_settings_ui = new ModbusSettingsUI(this);

    QTranslator qtLanguageTranslator;

    QGridLayout *gl1 = nullptr;
    QGridLayout *gl2 = nullptr;
    QGridLayout *gl3 = nullptr;
    QGridLayout *gl4 = nullptr;

    std::vector<QCheckBox*> di_v;
    std::vector<QCheckBox*> coil_v;
    std::vector<QDoubleSpinBox*> ir_v;
    std::vector<QDoubleSpinBox*> hr_v;
    std::vector<QLineEdit*> ir_le_v;
    std::vector<QLineEdit*> hr_le_v;

    std::map<int, std::map<QString, QModbusDataUnit>> _mdum;

    bool _request = false;
};
#endif // MAINWINDOW_H
