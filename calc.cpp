#include "calc.h"
#include "ui_calc.h"
#include <cmath>
calc::calc(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::calc)
{
    ui->setupUi(this);
    setFixedSize(geometry().width(),geometry().height());
}

calc::~calc()
{
    delete ui;
}

void calc::updatedBW(double in){
    if(lock_bw)return;
    lock_q = true;
    double q = round(1000000*pow(2,in*0.5)/(pow(2,in)-1))/1000000;
    ui->q->setValue(q);
    lock_q = false;
}
void calc::updatedQ(double q){
    if(lock_q)return;
    lock_bw = true;
    double QQ1st = ((2*q*q)+1)/(2*q*q);
    double QQ2nd = pow(2*QQ1st,2)/4;
    double bw = round(1000000*log(QQ1st+sqrt(QQ2nd-1))/log(2))/1000000;
    ui->bw->setValue(bw);
    lock_bw = false;
}
