#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ddccontext.h"
#include "addpoint.h"
#include "textpopup.h"
#include "calc.h"
#include <QFileDialog>
#include <QDebug>
#include <QAction>
#include <QMessageBox>
#include <vector>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    g_dcDDCContext = new class DDCContext;
    ui->listView_DDCPoints->setItemDelegate(new SaveItemDelegate());
    ui->listView_DDCPoints->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->graph->yAxis->setRange(QCPRange(-1, 1));
    ui->graph->xAxis->setRange(QCPRange(0, 24000));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::saveDDCProject()
{
    QString n("\n");
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "Save VDC Project File", "", "ViPER DDC Project (*.vdcprj)");
    if (fileName != "" && fileName != nullptr)
    {
        QFileInfo fi(fileName);
        QString ext = fi.suffix();
        if(ext!="vdcprj")fileName.append(".vdcprj");
        mtx.lock();
        try
        {
            QFile caFile(fileName);
            caFile.open(QIODevice::WriteOnly | QIODevice::Text);

            if(!caFile.isOpen()){
                qDebug() << "- Error, unable to open" << "outputFilename" << "for output";
            }
            QTextStream outStream(&caFile);
            outStream << "# ViPER-DDC Project File, v1.0.0.0" + n;
            outStream << "# Generated by DDCToolbox/Qt (@ThePBone)" + n;
            outStream << n;
            for (int i = 0; i < ui->listView_DDCPoints->rowCount(); i++)
            {
                outStream << "# Calibration Point " + QString::number(i + 1) + n;
                outStream << ui->listView_DDCPoints->item(i,0)->data(Qt::DisplayRole).toString() + "," + ui->listView_DDCPoints->item(i,1)->data(Qt::DisplayRole).toString() + "," + ui->listView_DDCPoints->item(i,2)->data(Qt::DisplayRole).toString() + n;
            }
            outStream << n;
            outStream << "#File End" + n;
            caFile.close();
        }
        catch (const std::exception& e)
        {
            qWarning() << e.what();
            mtx.unlock();
            return;
        }
        mtx.unlock();
    }
}
void MainWindow::loadDDCProject()
{

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Open VDC Project File", "", "ViPER DDC Project (*.vdcprj)");
    if (fileName != "" && fileName != nullptr){
        mtx.lock();

        try
        {
            QString str;
            QFile file(fileName);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
                QMessageBox::warning(this,"Error","Cannot open file for reading");
                return;
            }

            QTextStream in(&file);
            clearPoint();
            lock_actions = true;
            while (!in.atEnd())
            {
                str = in.readLine().trimmed();
                if (str != nullptr || str != "")
                {
                    if ((str.length() > 0) && !str.startsWith("#"))
                    {
                        QStringList strArray = str.split(',');
                        if ((!strArray.empty()) && (strArray.length() == 3))
                        {
                            int result = 0;
                            double num2 = 0.0;
                            double num3 = 0.0;
                            if ((sscanf(strArray[0].toUtf8().constData(), "%d", &result) == 1 &&
                                 sscanf(strArray[1].toUtf8().constData(), "%lf", &num2) == 1) &&
                                    sscanf(strArray[2].toUtf8().constData(), "%lf", &num3) == 1)
                            {
                                bool flag = false;
                                for (int i = 0; i < ui->listView_DDCPoints->rowCount(); i++)
                                {
                                    if (ui->listView_DDCPoints->item(i,0)->data(Qt::DisplayRole).toInt() == result)
                                    {
                                        flag = true;
                                        break;
                                    }
                                }
                                if (!flag)
                                {

                                    ui->listView_DDCPoints->setSortingEnabled(false);
                                    QTableWidgetItem *c1 = new QTableWidgetItem();
                                    QTableWidgetItem *c2 = new QTableWidgetItem();
                                    QTableWidgetItem *c3 = new QTableWidgetItem();
                                    c1->setData(Qt::DisplayRole, result);
                                    c2->setData(Qt::DisplayRole, num2);
                                    c3->setData(Qt::DisplayRole, num3);

                                    qDebug() << result << num2 << num3;

                                    ui->listView_DDCPoints->insertRow ( ui->listView_DDCPoints->rowCount() );
                                    ui->listView_DDCPoints->setItem(ui->listView_DDCPoints->rowCount()-1, 0, c1);
                                    ui->listView_DDCPoints->setItem(ui->listView_DDCPoints->rowCount()-1, 1, c2);
                                    ui->listView_DDCPoints->setItem(ui->listView_DDCPoints->rowCount()-1, 2, c3);
                                    ui->listView_DDCPoints->setSortingEnabled(true);

                                    g_dcDDCContext->AddFilter(result, num3, num2, 44100.0);
                                }
                            }
                        }
                    }
                }
            }
            file.close();

        }
        catch (const std::exception& e)
        {
            qWarning() << e.what();
            lock_actions=false;
            mtx.unlock();
            return;
        }
        mtx.unlock();
        lock_actions=false;
        ui->listView_DDCPoints->update();
        ui->listView_DDCPoints->sortItems(0,Qt::SortOrder::AscendingOrder);

        drawGraph();
    }
}
void MainWindow::exportVDC()
{
    QString n("\n");
    std::list<double> p1 = g_dcDDCContext->ExportCoeffs(44100.0);
    std::list<double> p2 = g_dcDDCContext->ExportCoeffs(48000.0);

    if (p1.empty() || p2.empty())
    {
        QMessageBox::warning(this,"Error","Failed to export to VDC");
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "Save VDC", "", "VDC File (*.vdc)");
    if (fileName != "" && fileName != nullptr)
    {
        QFileInfo fi(fileName);
        QString ext = fi.suffix();
        if(ext!="vdc")fileName.append(".vdc");
        mtx.lock();
        try
        {
            QFile caFile(fileName);
            caFile.open(QIODevice::WriteOnly | QIODevice::Text);

            if(!caFile.isOpen()){
                qDebug() << "- Error, unable to open" << "outputFilename" << "for output";
            }
            QTextStream outStream(&caFile);
            outStream << "SR_44100:";

            std::vector<double> v1;
            for (double const &d: p1)
                v1.push_back(d);
            std::vector<double> v2;
            for (double const &d: p2)
                v2.push_back(d);

            for (size_t i = 0; i < v1.size(); i++)
            {
                outStream << qSetRealNumberPrecision(16) << v1.at(i);
                if (i != (v1.size() - 1))
                    outStream << ";";
            }
            outStream << n;
            outStream << "SR_48000:";

            for (size_t i = 0; i < v2.size(); i++)
            {
                outStream << qSetRealNumberPrecision(16) << v2.at(i);
                if (i != (v2.size() - 1))
                    outStream << ";";
            }

            outStream << n;
            caFile.close();
        }
        catch (const std::exception& e)
        {
            qWarning() << e.what();
            return;
        }
    }
}
void MainWindow::clearPoint(){
    lock_actions=true;
    g_dcDDCContext->ClearFilters();
    ui->listView_DDCPoints->clear();
    ui->listView_DDCPoints->setRowCount(0);
    ui->listView_DDCPoints->reset();
    ui->listView_DDCPoints->setHorizontalHeaderLabels(QStringList() << "Frequency" << "Bandwidth" << "Gain");
    ui->graph->clearItems();
    ui->graph->clearGraphs();
    ui->graph->yAxis->setRange(QCPRange(-1, 1));
    ui->graph->xAxis->setRange(QCPRange(0, 24000));
    lock_actions=false;
}
void MainWindow::editCell(QTableWidgetItem* item){
    if(lock_actions)return;


    int row = item->row();
    int result = 0;
    int nOldFreq = 0;
    double calibrationPointBandwidth = 0.0;
    double calibrationPointGain = 0.0;

    if(ui->listView_DDCPoints->rowCount() <= 0){
        qDebug() << "No data to edit!";
        return;
    }

    if ((sscanf(ui->listView_DDCPoints->item(row,0)->data(Qt::DisplayRole).toString().toUtf8().constData(), "%d", &result) == 1 &&
         sscanf(ui->listView_DDCPoints->item(row,1)->data(Qt::DisplayRole).toString().toUtf8().constData(), "%lf", &calibrationPointBandwidth) == 1) &&
            sscanf(ui->listView_DDCPoints->item(row,2)->data(Qt::DisplayRole).toString().toUtf8().constData(), "%lf", &calibrationPointGain) == 1){

        ui->listView_DDCPoints->setSortingEnabled(false);
        nOldFreq = Global::old_freq;


        //Validate frequency value
        if(result < 0){
            ui->listView_DDCPoints->item(row,0)->setData(Qt::DisplayRole,0);
            QMessageBox::warning(this,"Warning","Frequency value '" + QString::number(result) + "' is too low (0.0 ~ 24000.0)");
            result = 0;
        }
        else if(result > 24000){
            ui->listView_DDCPoints->item(row,0)->setData(Qt::DisplayRole,24000);
            QMessageBox::warning(this,"Warning","Frequency value '" + QString::number(result) + "' is too high (0.0 ~ 24000.0)");
            result = 24000;
        }

        //Validate bandwidth value
        else if(calibrationPointBandwidth < 0){
            ui->listView_DDCPoints->item(row,1)->setData(Qt::DisplayRole,0);
            QMessageBox::warning(this,"Warning","Bandwidth value '" + QString::number(calibrationPointBandwidth) + "' is too low (0.0 ~ 100.0)");
            calibrationPointBandwidth = 0;
        }
        else if(calibrationPointBandwidth > 100){
            ui->listView_DDCPoints->item(row,1)->setData(Qt::DisplayRole,100);
            QMessageBox::warning(this,"Warning","Bandwidth value '" + QString::number(calibrationPointBandwidth) + "' is too high (0.0 ~ 100.0)");
            calibrationPointBandwidth = 100;
        }

        //Validate gain value
        else if(calibrationPointGain < -24){
            ui->listView_DDCPoints->item(row,2)->setData(Qt::DisplayRole,-24);
            QMessageBox::warning(this,"Warning","Gain value '" + QString::number(calibrationPointGain) + "' is too low (-24.0 ~ 24.0)");
            calibrationPointGain = -24;
        }
        else if(calibrationPointGain > 24){
            ui->listView_DDCPoints->item(row,2)->setData(Qt::DisplayRole,24);
            QMessageBox::warning(this,"Warning","Gain value '" + QString::number(calibrationPointGain) + "' is too high (-24.0 ~ 24.0)");
            calibrationPointGain = 24;
        }

        //Check for duplicate frequencies
        for (int i = 0; i < ui->listView_DDCPoints->rowCount(); i++)
        {
            if (ui->listView_DDCPoints->item(i,0)->data(Qt::DisplayRole).toString() == ui->listView_DDCPoints->item(row,0)->data(Qt::DisplayRole).toString() && row != i)
            {
                ui->listView_DDCPoints->item(row,0)->setData(Qt::DisplayRole,Global::old_freq);
                QMessageBox::warning(this,"Error","Point '" + ui->listView_DDCPoints->item(i,0)->data(Qt::DisplayRole).toString() + "' already exists");
                return;
            }
        }

        mtx.lock();

        g_dcDDCContext->ModifyFilter(nOldFreq, result, calibrationPointGain, calibrationPointBandwidth, 44100.0);
        ui->listView_DDCPoints->setSortingEnabled(true);
        ui->listView_DDCPoints->sortItems(0);
        mtx.unlock();
        qDebug() << "Drawing...";
        drawGraph();
    }
    else qDebug() << "Invalid input data";
}
void MainWindow::addPoint(){
    addpoint *dlg = new addpoint;
    if(dlg->exec()){
        std::list<double> rawdata = dlg->returndata();
        std::vector<double> data(rawdata.begin(), rawdata.end());
        int freq = (int)data.at(0);
        double bw = data.at(1);
        double gain = data.at(2);

        for (int i = 0; i < ui->listView_DDCPoints->rowCount(); i++)
        {
            if (ui->listView_DDCPoints->item(i,0)->data(Qt::DisplayRole).toInt() == freq)
            {
                QMessageBox::warning(this,"Error","Point already exists");
                return;
            }
        }


        lock_actions=true;
        ui->listView_DDCPoints->setSortingEnabled(false);

        QTableWidgetItem *c1 = new QTableWidgetItem();
        QTableWidgetItem *c2 = new QTableWidgetItem();
        QTableWidgetItem *c3 = new QTableWidgetItem();
        c1->setData(Qt::DisplayRole, freq);
        c2->setData(Qt::DisplayRole, bw);
        c3->setData(Qt::DisplayRole, gain);
        ui->listView_DDCPoints->insertRow ( ui->listView_DDCPoints->rowCount() );
        ui->listView_DDCPoints->setItem(ui->listView_DDCPoints->rowCount()-1, 0, c1);
        ui->listView_DDCPoints->setItem(ui->listView_DDCPoints->rowCount()-1, 1, c2);
        ui->listView_DDCPoints->setItem(ui->listView_DDCPoints->rowCount()-1, 2, c3);
        ui->listView_DDCPoints->setSortingEnabled(true);

        lock_actions=false;

        mtx.lock();
        g_dcDDCContext->AddFilter(freq, gain, bw, 44100.0);
        mtx.unlock();
        drawGraph();
    }
}
void MainWindow::removePoint(){
    lock_actions=true;
    mtx.lock();
    if (ui->listView_DDCPoints->currentRow() < 0)
        mtx.unlock();
    else
    {
        ui->listView_DDCPoints->setSortingEnabled(false);
        QList<int> removeRows;
        const QModelIndexList list = ui->listView_DDCPoints->selectionModel()->selectedRows();
        for (int i = 0; i < list.count(); i++)
        {
            QModelIndex index = list.at(i);
            int row = index.row();
            removeRows.append(row);
            int freq = ui->listView_DDCPoints->item(row,0)->data(Qt::DisplayRole).toInt();
            g_dcDDCContext->RemoveFilter(freq);
        }
        for(int i=0;i<removeRows.count();++i)
        {
            for(int j=i;j<removeRows.count();++j) {
                if(removeRows.at(j) > removeRows.at(i)) {
                    removeRows[j]--;
                }
            }
            ui->listView_DDCPoints->model()->removeRows(removeRows.at(i), 1);
        }
        ui->listView_DDCPoints->setSortingEnabled(true);
        ui->listView_DDCPoints->sortItems(0);

        mtx.unlock();
        drawGraph();
    }
    lock_actions=false;
}
void MainWindow::drawGraph(){
    int bandCount = 240;
    std::vector<float> responseTable = g_dcDDCContext->GetResponseTable(bandCount, 44100.0);
    if (responseTable.size()<=0)
    {
        return;
    }
    float num2 = 0.0f;
    for (size_t i = 0; i < (size_t)bandCount; i++)
    {
        if (abs(responseTable.at(i)) > num2)
        {
            num2 = abs(responseTable.at(i));
        }
    }
    if (num2 <= 1E-08f)
    {
        qDebug() << "Cannot draw graph. Currently at line: " << __LINE__;
        ui->graph->clearItems();
        ui->graph->clearGraphs();
    }
    else
    {
        if (num2 > 24.0f)
        {
            float num4 = 24.0f / num2;
            for (size_t n = 0; n < (size_t)bandCount; n++)
            {
                responseTable.at(n) *= num4;
            }
        }
        for (size_t k = 0; k < (size_t)bandCount; k++)
            responseTable.at(k) /= 24.0f;

        ui->graph->clearPlottables();
        ui->graph->clearItems();
        ui->graph->clearGraphs();
        ui->graph->yAxis->setRange(QCPRange(-1, 1));
        QCPGraph *plot = ui->graph->addGraph();

        for (size_t m = 0; m < (size_t)bandCount; m++)
        {
            plot->addData(m*100,(double)responseTable.at(m));//m * 10 -> to fit the scale to 24000hz
            ui->graph->xAxis->setRange(QCPRange(0, m*100));
        }
    }
    ui->graph->replot();
}
void MainWindow::showIntroduction(){
    QString data = "Unable to open HTML file";
    QFile file(":/html/introduction.html");
    if(!file.open(QIODevice::ReadOnly))
        qDebug()<<"Unable to open HTML file. Line: " << __LINE__;
    else
        data = file.readAll();
    file.close();
    TextPopup *t = new TextPopup(data);
    t->show();
}
void MainWindow::showKeycombos(){
    QString data = "Unable to open HTML file";
    QFile file(":/html/keycombos.html");
    if(!file.open(QIODevice::ReadOnly))
        qDebug()<<"Unable to open HTML file. Line: " << __LINE__;
    else
        data = file.readAll();
    file.close();
    TextPopup *t = new TextPopup(data);
    t->show();
}
void MainWindow::showCalc(){
    calc *t = new calc();
    t->show();
}
void MainWindow::importParametricAutoEQ(){

    ui->listView_DDCPoints->clear();

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Import AutoEQ config 'ParametricEQ.txt'", "", "AutoEQ ParametricEQ.txt (*ParametricEQ.txt);;All files (*.*)");
    if (fileName != "" && fileName != nullptr){
        QString str;
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
            QMessageBox::warning(this,"Error","Cannot open file for reading");
            return;
        }

        clearPoint();
        QTextStream in(&file);

        while (!in.atEnd())
        {
            str = in.readLine().trimmed();
            if (str != nullptr || str != "")
            {
                if ((str.length() > 0) && !str.startsWith("#"))
                {
                    QString strPart2 = str.split(':')[1].trimmed();
                    QStringList lineParts = strPart2.split(" ");
                    /**
                      [0] "ON"
                      [1] "PK"
                      [2] "Fc"
                      [3] <Freq,INT>
                      [4] "Hz"
                      [5] "Gain"
                      [6] <Gain,FLOAT>
                      [7] "dB"
                      [8] "Q"
                      [9] <Q-Value,FLOAT>
                                          **/
                    if ((!lineParts.empty()) && (lineParts.length() == 10))
                    {
                        int freq = lineParts[3].toInt();
                        float gain = lineParts[6].toFloat();
                        float q = lineParts[9].toFloat();
                        if(freq < 0)return;

                        //Convert Q to BW
                        float QQ1st = ((2*q*q)+1)/(2*q*q);
                        double QQ2nd = pow(2*QQ1st,2)/4;
                        double bw = round(1000000*log(QQ1st+sqrt(QQ2nd-1))/log(2))/1000000;
                        qDebug() << bw;

                        bool flag = false;
                        for (int i = 0; i < ui->listView_DDCPoints->rowCount(); i++)
                        {
                            if (ui->listView_DDCPoints->item(i,0)->data(Qt::DisplayRole).toInt() == freq)
                            {
                                flag = true;
                                break;
                            }
                        }
                        if (!flag)
                        {
                            lock_actions = true;
                            ui->listView_DDCPoints->setSortingEnabled(false);
                            QTableWidgetItem *c1 = new QTableWidgetItem();
                            QTableWidgetItem *c2 = new QTableWidgetItem();
                            QTableWidgetItem *c3 = new QTableWidgetItem();
                            c1->setData(Qt::DisplayRole, freq);
                            c2->setData(Qt::DisplayRole, bw);
                            c3->setData(Qt::DisplayRole, (double)gain);

                            ui->listView_DDCPoints->insertRow ( ui->listView_DDCPoints->rowCount() );
                            ui->listView_DDCPoints->setItem(ui->listView_DDCPoints->rowCount()-1, 0, c1);
                            ui->listView_DDCPoints->setItem(ui->listView_DDCPoints->rowCount()-1, 1, c2);
                            ui->listView_DDCPoints->setItem(ui->listView_DDCPoints->rowCount()-1, 2, c3);
                            ui->listView_DDCPoints->setSortingEnabled(true);
                            lock_actions = false;
                            g_dcDDCContext->AddFilter(freq, (double)gain, bw, 44100.0);
                        }
                    }
                }
            }
        }
        ui->listView_DDCPoints->sortItems(0,Qt::SortOrder::AscendingOrder);
        drawGraph();
        file.close();
    }

}
