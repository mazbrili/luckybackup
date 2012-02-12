/* ---------------------------------- scheduleDialog.cpp ---------------------------------------------------------------------------
Display a dialog. Schedules profiles via cron

===============================================================================================================================
===============================================================================================================================
     This file is part of "luckyBackup" project
     Copyright 2008-2011, Loukas Avgeriou
     luckyBackup is distributed under the terms of the GNU General Public License
     luckyBackup is free software: you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation, either version 3 of the License, or
     (at your option) any later version.

     luckyBackup is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.
 
     You should have received a copy of the GNU General Public License
     along with luckyBackup.  If not, see <http://www.gnu.org/licenses/>.


 project version    : Please see "main.cpp" for project version

 developer          : luckyb 
 last modified      : 30 Aug 2010

===============================================================================================================================
===============================================================================================================================
*/


#include "scheduleDialog.h"
#include "scheduleClass.h"

int const maxSchedule = 20;		//The maximum number or profiles schedule permitted
schedule *Schedule[maxSchedule];	//The array that holds name etc of "schedule list"
QFile scheduleFile (schedulefilename );
QProcess *cronProcess;		//crontab process

// class scheduleDialog Constructor=================================================================================================
// schedule existing profiles to execute via cron
scheduleDialog::scheduleDialog (QDialog *parent) : QDialog (parent)
{
	uiS.setupUi(this);

	//variables initialization
	TotalSchedule=0;		//The number of available profiles schedulesin "schedule list"- starts from 1
	currentSchedule=0;		//this holds the current row from the "schedule list"- starts from 0
	noSchedules = TRUE;		//becomes true when no schedules are declared
	changesMade = FALSE;		//becomes true if changes are made but no cronIt is pressed

	//connections ----------------------------------------------------------------------------------------------------
	connect(uiS.pushButton_schedule, SIGNAL( pressed() ), this, SLOT(schedulePressed() ) );		//cronIT pushButton
	connect(uiS.pushButton_cancel, SIGNAL( pressed() ), this, SLOT(cancelPressed() ) );		//cancel pushButton

	connect(uiS.pushButton_add, SIGNAL( pressed() ), this, SLOT(addPressed() ) );			//add command pushButton
	connect(uiS.pushButton_remove, SIGNAL( pressed() ), this, SLOT(removePressed() ) );		//remove command pushButton
	connect(uiS.pushButton_modify, SIGNAL( pressed() ), this, SLOT(modifyPressed() ) );		//modify command pushButton
	
	connect(uiS.pushButton_viewCrontab, SIGNAL( pressed() ), this, SLOT(viewCrontab() ) );		//view curreent crontab command pushButton
	
	connect(uiS.pushButton_modifyOkay, SIGNAL( pressed() ), this, SLOT(okaySchedulePressed() ) );		//modify okay pushButton
	connect(uiS.pushButton_modifyCancel, SIGNAL( pressed() ), this, SLOT(modifyCancelPressed() ) );		//modify cancel pushButton

	connect(uiS.listWidget_schedule , SIGNAL( currentRowChanged(int) ), this, SLOT(fillDetails() ) );	//list item selection changed

	
	cronProcess = new QProcess(this);	//create a new qprocess (for crontab) & connect signals
	connect(cronProcess, SIGNAL(readyReadStandardError()), this, SLOT(appendCrontabOutput()));
	connect(cronProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(appendCrontabOutput()));

	//initialize view of widgets
	uiS.groupBox_addProfileSchedule -> setVisible(FALSE);
	uiS.listWidget_schedule -> setEnabled(TRUE);
	uiS.pushButton_add -> setEnabled(TRUE);
	uiS.pushButton_modify -> setEnabled(TRUE);
	uiS.pushButton_remove -> setEnabled(TRUE);
	uiS.pushButton_cancel -> setEnabled(TRUE);
	uiS.pushButton_schedule -> setEnabled(TRUE);
	
	//profile combobox initialization
	uiS.comboBox_profile -> clear();
	currentProfileIndex = -1;
	QDir profiledir(profileDir);
	profiledir.setFilter(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
	profiledir.setSorting(QDir::Name | QDir::LocaleAware);
	QStringList profileNames = profiledir.entryList(QStringList("*.profile"));
	QString profilename="";
	for (count = 0; count < profileNames.size(); ++count)
	{
		profilename = profileNames[count];
		if (currentProfile == profileDir + profilename)	// set the currentProfileIndex to the index of the current profile
			currentProfileIndex = count;
		profilename.chop(8);
		uiS.comboBox_profile -> addItem (profilename);
	}
	if (currentProfileIndex == -1)	// if no default profile exists
		currentProfileIndex = 0;	
	
	//open the schedule.dat file and fill arrays & schedule list
	if (!scheduleFile.open(QIODevice::ReadOnly))	//if there is an error opening it, delete it to create a new one
		scheduleFile.remove();
	else
	{
		QDataStream in(&scheduleFile);
		in.setVersion(QDataStream::Qt_4_3);

		QVariant v;					//we will import everything as QVariant using this temp variable
		QString vString;				//temp variable to import "labels" of real data
		QString tempAppName = "asxeto";
		double tempAppVersion=0;
		
		in>>v;	if (v.toString()=="appName")
			{in >> v;	tempAppName = v.toString();}	//input the application name & version--------------------------
		in>>v;	if (v.toString()=="appVersion")
			{in >> v;	tempAppVersion = v.toDouble();}
		
		if ( (tempAppName != appName) || (tempAppVersion < validScheduleVersion) )//check if the file is a valid luckybackup schedule file
		{
			message = tr("The existent schedule file is not a valid luckyBackup v.","v is for version")
				+ countStr.setNum(appVersion) + tr(" file","BEWARE of the whitespace before file")+"<br><br>"+tr("Starting from scratch")+"...<br>" ;
			textDialog textdialogW ("QtWarning", message, this);
			textdialogW.exec();
			scheduleFile.remove();
			return;
		}

		in>>v;	if (v.toString()=="TotalSchedules")
			{in >> v;	TotalSchedule = v.toInt();}	//input the size of the schedule list first

		currentSchedule = 0;	vString="";
		while (currentSchedule < TotalSchedule)
		{
			schedule *tempSchedule = new schedule;

			in>>v;	vString = v.toString();	in >> v;	//input a label in vString and real data in v
			
			while (vString != "current schedule end")
			{
				if (vString == "ScheduleName")		tempSchedule	-> SetName(v.toString());

				if (vString == "ProfilePath")		tempSchedule	-> SetProfilePath(v.toString());
				if (vString == "ProfileName")		tempSchedule	-> SetProfileName(v.toString());

				if (vString == "ScheduleMonth")		tempSchedule	-> SetMonth(v.toInt());
				if (vString == "ScheduleMonthDay")	tempSchedule	-> SetMonthDay(v.toInt());
				if (vString == "ScheduleWeekDay")	tempSchedule	-> SetWeekDay(v.toInt());
				if (vString == "ScheduleHour")		tempSchedule	-> SetHour(v.toInt());
				if (vString == "ScheduleMinute")	tempSchedule	-> SetMinute(v.toInt());

				if (vString == "ScheduleSkipCritical")	tempSchedule	-> SetSkipCritical(v.toBool());
				if (vString == "ScheduleConsoleMode")	tempSchedule	-> SetConsoleMode(v.toBool());
				if (vString == "ScheduleAtReboot")	tempSchedule	-> SetReboot(v.toBool());
                if (vString == "ScheduleAtRebootDelay")  tempSchedule    -> SetDelayReboot(v.toInt());

				in>>v;	vString = v.toString();
				if (vString!="current schedule end")
					in >> v;
				else
					Schedule[currentSchedule] = tempSchedule;
				
			}
			uiS.listWidget_schedule -> addItem(scheduleText(Schedule[currentSchedule] -> GetProfileName(),
									Schedule[currentSchedule] -> GetMonth(),
									Schedule[currentSchedule] -> GetMonthDay(),
									Schedule[currentSchedule] -> GetWeekDay(),
									Schedule[currentSchedule] -> GetHour(),
									Schedule[currentSchedule] -> GetMinute(),
									Schedule[currentSchedule] -> GetReboot() ));
			currentSchedule++;
		}
		//TotalSchedule = uiS.listWidget_schedule -> count();	//Get the schedule list size

		if (TotalSchedule > 0)
		{
			uiS.listWidget_schedule -> setCurrentRow(0);
			noSchedules = FALSE;
		}
		else
			uiS.listWidget_schedule -> addItem(tr("No schedules are declared !!"));
		scheduleFile.close();					//close the file
	}
	
	// use this cause when the dialog starts and ther's only 1 schedule the filldetails cannot be called !!
	currentSchedule = uiS.listWidget_schedule -> currentRow();		//current operations list row number
	fillDetails();
}
//===================================================================================================================================================
//------------------------------------------------------------------SLOTS----------------------------------------------------------------------------
//===================================================================================================================================================


// CLOSE button pressed=====================================================================================================
void scheduleDialog::cancelPressed()
{
	close();	//emmit a QcloseEvent
}

//window closebutton pressed or QcloseEvent emitted==========================================================================
void scheduleDialog::closeEvent(QCloseEvent *event)
{
	if (changesMade)
	{
		textDialog textdialogQ ("QtQuestion", tr("You have made changes without clicking the cronIT button") + ".<br>" +
					tr("Are you sure you want to close the schedule dialog ?"), this);
		textdialogQ.exec();

		if (textdialogQ.getGoOn() > 0)		//if user answers yes
			event->accept();
		else
			event->ignore();
	}
	else
		event->accept();
}


// schedule button pressed=======================================================================================================
// Read all fields of dialog, fill appropriate variables, saves crontab file and creates a cron job
void scheduleDialog::schedulePressed()
{
	if (noSchedules)			// if no schedules are declared, remove the info line first
		uiS.listWidget_schedule -> takeItem (0);

	TotalSchedule = uiS.listWidget_schedule -> count();	//Get the schedule list size

	//update schedule file ------------------------------------------------------------------------------------------
	if (scheduleFile.exists())		//if a schedule file exists delete it
		scheduleFile.remove();

	if (noSchedules)			// if no schedules are declared, remove the info line
		uiS.listWidget_schedule -> takeItem (0);

	if (!scheduleFile.open(QIODevice::WriteOnly))	//create a new schedule file
	{
		textDialog textdialogI ("QtInformation", tr("Unable to create schedule file")+" !!<br><br>"+ scheduleFile.errorString(), this);
		textdialogI.exec();
	}
	else 		//write some stuff to schedule file
	{
 		QDataStream out(&scheduleFile);
		out.setVersion(QDataStream::Qt_4_3);

		QVariant v;					//we will export everything as QVariant using this temp variable
		v = "appName";		out << v;	v = appName;		out << v;		//output the application name 
		v = "appVersion";	out << v;	v = appVersion;		out << v;		//output the application version 
		v = "TotalSchedules";	out << v;	v = TotalSchedule;	out << v;		//output the size of the schedule list 
		
		currentSchedule = 0;
		while (currentSchedule < TotalSchedule)
		{
			v = "ScheduleName";	out << v;	v = Schedule[currentSchedule] -> GetName();			out << v;

			v = "ProfilePath";	out << v;	v = Schedule[currentSchedule] -> GetProfilePath();		out << v;
			v = "ProfileName";	out << v;	v = Schedule[currentSchedule] -> GetProfileName();		out << v;

			v = "ScheduleMonth";	out << v;	v = Schedule[currentSchedule] -> GetMonth();			out << v;
			v = "ScheduleMonthDay";	out << v;	v = Schedule[currentSchedule] -> GetMonthDay();			out << v;
			v = "ScheduleWeekDay";	out << v;	v = Schedule[currentSchedule] -> GetWeekDay();			out << v;
			v = "ScheduleHour";	out << v;	v = Schedule[currentSchedule] -> GetHour();			out << v;
			v = "ScheduleMinute";	out << v;	v = Schedule[currentSchedule] -> GetMinute();			out << v;

			v = "ScheduleSkipCritical";out << v;	v = Schedule[currentSchedule] -> GetSkipCritical();		out << v;
			v = "ScheduleConsoleMode";out << v;	v = Schedule[currentSchedule] -> GetConsoleMode();		out << v;
			v = "ScheduleAtReboot";	out << v;	v = Schedule[currentSchedule] -> GetReboot();			out << v;
            v = "ScheduleAtRebootDelay"; out << v;   v = Schedule[currentSchedule] -> GetDelayReboot();           out << v;

			v = "current schedule end";	out << v;
			
			currentSchedule++;
		}
		
		v = "all schedules end";	out << v;
		scheduleFile.close();	//close the file
	}
	
	createCron();		//create a cronfile & update user's crontab
	crontabUpdated = TRUE;
	changesMade = FALSE;

	textDialog textdialogI ("QtInformation",  "<font color=blue><b>" + currentUser + "</b></font><br><br>" +
				tr("Your crontab is updated successfully"), this);
	textdialogI.exec();
	
	//close();		//emmit a QcloseEvent
}

// add button pressed=====================================================================================================
// shows part of the widget to add a new profile schedule
void scheduleDialog::addPressed()
{
	if (noSchedules)			// if no schedules are declared, remove the info line first
	{
		uiS.listWidget_schedule -> takeItem (0);
		noSchedules = FALSE;
	}

	TotalSchedule = uiS.listWidget_schedule -> count();	//Get the schedule list size
	currentSchedule = TotalSchedule;			//We will add an item !!
	if ( (TotalSchedule) >= (maxSchedule-1) )	//if the maxSchedulelimit-1 is reached 
	{
		QMessageBox::warning(this, appName,
				tr("You have reached the maximum limit of supported scheduled profiles")+ "<br><br>"+
				tr("You have to remove an item if you wish to add another")+" !!",
				QMessageBox::Ok | QMessageBox::Default);
		return;
	}
	
	uiS.groupBox_addProfileSchedule -> setVisible(TRUE);
	uiS.listWidget_schedule -> setEnabled(FALSE);
	uiS.groupBox_addProfileSchedule -> setTitle(tr("Add profile schedule"));
	uiS.pushButton_add -> setEnabled(FALSE);
	uiS.pushButton_modify -> setEnabled(FALSE);
	uiS.pushButton_remove -> setEnabled(FALSE);
	uiS.pushButton_cancel -> setEnabled(FALSE);
	uiS.pushButton_schedule -> setEnabled(FALSE);
	uiS.pushButton_viewCrontab -> setEnabled(FALSE);
	clearScheduleDetails();
}

// modify button pressed=====================================================================================================
// modifies a profile schedule
void scheduleDialog::modifyPressed()
{
	currentSchedule = uiS.listWidget_schedule -> currentRow();		//current operations list row number
	if ((currentSchedule < 0) || (noSchedules))				//if nothing is selected or no schedules declared, do nothing
		return;

	uiS.groupBox_addProfileSchedule -> setVisible(TRUE);
	uiS.groupBox_addProfileSchedule -> setTitle(tr("Modify profile schedule"));
	uiS.listWidget_schedule -> setEnabled(FALSE);
	uiS.pushButton_add -> setEnabled(FALSE);
	uiS.pushButton_modify -> setEnabled(FALSE);
	uiS.pushButton_remove -> setEnabled(FALSE);
	uiS.pushButton_cancel -> setEnabled(FALSE);
	uiS.pushButton_schedule -> setEnabled(FALSE);
	uiS.pushButton_viewCrontab -> setEnabled(FALSE);
}


// List item Remove button pressed==============================================================================================
// Removes an item from the schedule list
void scheduleDialog::removePressed()
{
	int selected;

	//schedule list remove button
	selected = uiS.listWidget_schedule -> currentRow();	//current list row number
		if (selected < 0)				//if nothing is selected do nothing
			return;
	uiS.listWidget_schedule -> takeItem(selected);

	TotalSchedule = uiS.listWidget_schedule -> count();	//Get the schedule list size
	if (TotalSchedule == 0)
	{
		noSchedules = TRUE;
		uiS.listWidget_schedule -> addItem(tr("No schedules are declared !!"));
	}
	changesMade = TRUE;
}

// current button pressed==============================================================================================
// Displays the current crontab
void scheduleDialog::viewCrontab()
{
	// copy user's crontab to QString CronTab
	CronTab = "";
	cronProcess -> start ("crontab",QStringList() << "-l");	// execute crontab -l command & capture output to QString CronTab
	cronProcess -> waitForStarted();
	cronProcess -> waitForFinished();

	if (CronTab == "")
		CronTab = currentUser + " - " + tr("no crontab entries exist");
	else
		CronTab.prepend("User: " + currentUser + " - " + tr("crontab content") + "\n\n");
	textDialog textdialog ("ViewCurrentCrontab", CronTab, this);
	textdialog.exec();
}

// modify cancel button pressed=================================================================================================
void scheduleDialog::modifyCancelPressed()
{
	uiS.groupBox_addProfileSchedule -> setTitle(tr("Profile schedule details"));
	uiS.groupBox_addProfileSchedule -> setVisible(FALSE);
	uiS.listWidget_schedule -> setEnabled(TRUE);
	uiS.pushButton_add -> setEnabled(TRUE);
	uiS.pushButton_modify -> setEnabled(TRUE);
	uiS.pushButton_remove -> setEnabled(TRUE);
	uiS.pushButton_cancel -> setEnabled(TRUE);
	uiS.pushButton_schedule -> setEnabled(TRUE);
	uiS.pushButton_viewCrontab -> setEnabled(TRUE);

	TotalSchedule = uiS.listWidget_schedule -> count();	//Get the schedule list size
	if (TotalSchedule == 0)
	{
		noSchedules = TRUE;
		uiS.listWidget_schedule -> addItem(tr("No schedules are declared !!"));
	}
}

// cancelSchedulePressed button pressed===============================================================================================
// sets the Profile Schedule details fields to default values
void scheduleDialog::clearScheduleDetails()
{
	//initialize all fields
	uiS.comboBox_profile -> setCurrentIndex(currentProfileIndex);
	uiS.comboBox_month -> setCurrentIndex(0);
	uiS.comboBox_dayOfMonth -> setCurrentIndex(0);
	uiS.comboBox_DayOfWeek -> setCurrentIndex(0);
	uiS.spinBox_hour -> setValue(0);
	uiS.spinBox_minute -> setValue(0);
    uiS.spinBox_delayReboot -> setValue(3);
	uiS.checkBox_skipCritical -> setCheckState(Qt::Checked);
	uiS.checkBox_consoleMode -> setCheckState(Qt::Unchecked);
	uiS.checkBox_reboot -> setCheckState(Qt::Unchecked);
}

// fillDetails=====================================================================================================
//function to fill the fields of schedule details from arrays
void scheduleDialog::fillDetails()
{
	currentSchedule = uiS.listWidget_schedule -> currentRow();		//current operations list row number
	if ((currentSchedule < 0) || (noSchedules))				//if nothing is selected or no schedules declared, do nothing
		return;

	//read array currentSchedule element data & fill add fields
	uiS.comboBox_profile -> setCurrentIndex( uiS.comboBox_profile -> findText(Schedule[currentSchedule] -> GetProfileName()) );
	uiS.comboBox_month -> setCurrentIndex(Schedule[currentSchedule] -> GetMonth());
	uiS.comboBox_dayOfMonth -> setCurrentIndex(Schedule[currentSchedule] -> GetMonthDay());
	uiS.comboBox_DayOfWeek -> setCurrentIndex(Schedule[currentSchedule] -> GetWeekDay());
	uiS.spinBox_hour -> setValue(Schedule[currentSchedule] -> GetHour());
	uiS.spinBox_minute -> setValue(Schedule[currentSchedule] -> GetMinute());
    uiS.spinBox_delayReboot -> setValue(Schedule[currentSchedule] -> GetDelayReboot());
	
	if (Schedule[currentSchedule] -> GetSkipCritical())
		uiS.checkBox_skipCritical -> setCheckState(Qt::Checked);
	else
		uiS.checkBox_skipCritical -> setCheckState(Qt::Unchecked);
	
	if (Schedule[currentSchedule] -> GetConsoleMode())
		uiS.checkBox_consoleMode -> setCheckState(Qt::Checked);
	else
		uiS.checkBox_consoleMode -> setCheckState(Qt::Unchecked);
	if (Schedule[currentSchedule] -> GetReboot())
		uiS.checkBox_reboot -> setCheckState(Qt::Checked);
	else
		uiS.checkBox_reboot -> setCheckState(Qt::Unchecked);
}

// okaySchedulePressed=====================================================================================================
// adss a new profile scedule to the list or modifies an existing one
void scheduleDialog::okaySchedulePressed()
{
	noSchedules = FALSE;
	schedule *tempOp = new schedule;

	//check valid & existent profile*****************************************************
	QFile proFile (profileDir + uiS.comboBox_profile -> currentText() + ".profile");
	if (!proFile.open(QIODevice::ReadOnly))
	{
		QMessageBox::information(this, appName, tr("Invalid profile")+"<br><br>" + proFile.errorString());
		return;
	}
/*	I have disabled this check as TOO much. If you wish to re-enable it, use the new profile "text format" from loadcurrentprofile()
	else	//read the file to check if it's valid
	{
		QDataStream in(&proFile);
		in.setVersion(QDataStream::Qt_4_3);

		QVariant v;					//we will import everything as QVariant using this temp variable
		QString vString;				//temp variable to import "labels" of real data
		QString tempAppName = "asxeto";
		double tempAppVersion=0;
		in>>v;	if (v.toString()=="appName")
			in >> v;	tempAppName = v.toString();	//input the application name & version--------------------------
		in>>v;	if (v.toString()=="appVersion")
			in >> v;	tempAppVersion = v.toDouble();
		if ( (tempAppName != appName) || (tempAppVersion < validProfileVersion) )//check if the file is a valid luckybackup profile
		{
			message = tr("The profile you are trying to open is not a valid luckyBackup v.","v is for version")+" "
				+ countStr.setNum(appVersion) + tr(" profile","BEWARE of the whitespace in the beginning")+"<br><br>";
			QMessageBox::warning(this, appName, message, QMessageBox::Ok | QMessageBox::Default);
			return;
		}
	}
*/	proFile.close();

	//read fields & fill array
	tempOp -> SetProfileName (uiS.comboBox_profile -> currentText());
	tempOp -> SetProfilePath (profileDir + uiS.comboBox_profile -> currentText() + ".profile");

	tempOp -> SetReboot (uiS.checkBox_reboot -> isChecked());
	tempOp -> SetMonth (uiS.comboBox_month -> currentIndex());
	tempOp -> SetMonthDay (uiS.comboBox_dayOfMonth -> currentIndex());
	tempOp -> SetWeekDay (uiS.comboBox_DayOfWeek -> currentIndex());
	tempOp -> SetHour (uiS.spinBox_hour -> value());
	tempOp -> SetMinute (uiS.spinBox_minute -> value());
    tempOp -> SetDelayReboot (uiS.spinBox_delayReboot -> value());
	tempOp -> SetSkipCritical (uiS.checkBox_skipCritical -> isChecked());
	tempOp -> SetConsoleMode (uiS.checkBox_consoleMode -> isChecked());
	tempOp -> SetName (scheduleText(tempOp -> GetProfileName(),
			tempOp -> GetMonth(),
			tempOp -> GetMonthDay(),
			tempOp -> GetWeekDay(),
			tempOp -> GetHour(),
			tempOp -> GetMinute(),
			tempOp -> GetReboot()));
	Schedule[currentSchedule] = tempOp;

	if (currentSchedule == TotalSchedule)
	{
		uiS.listWidget_schedule -> addItem( Schedule[currentSchedule] -> GetName() );	//add item at schedule list
		uiS.listWidget_schedule -> setCurrentRow(currentSchedule);
	}
	else	//take out the selected item from the list & insert a new one at the same position with the new name
		(uiS.listWidget_schedule -> currentItem()) -> setText(Schedule[currentSchedule] -> GetName());

	TotalSchedule = uiS.listWidget_schedule -> count();		//set the TotalSchedule to what it is now

	modifyCancelPressed();
	changesMade = TRUE;
}

// scheduleText=====================================================================================================
// returns the text displayed at the schedule list (eg execute profile default at .... every...)
QString scheduleDialog::scheduleText(QString name,int month,int monthday, int weekday, int hour, int minute, bool reboot)
{
	QString tempProfileName = tr("Execute profile")+" '"+ name + "', ";
	if (reboot)
	{
		tempProfileName.append(tr("once, at system startup","full phrase: Execute profile xxxxx, once at system startup"));
		return tempProfileName;
	}
	
	
	tempProfileName.append(tr("at")+" ");
	if (hour < 10)
		tempProfileName.append("0"+countStr.setNum(hour) + ":");
	else
		tempProfileName.append(countStr.setNum(hour) + ":");
	if (minute < 10) 
		tempProfileName.append("0"+countStr.setNum(minute));
	else
		tempProfileName.append(countStr.setNum(minute));
	tempProfileName.append(", "+tr("every")+" ");

	if ((weekday==0) && (monthday== 0))
		tempProfileName.append(tr("day"));
	if (weekday==1)
		tempProfileName.append(tr("Sunday"));
	if (weekday==2)
		tempProfileName.append(tr("Monday"));
	if (weekday==3)
		tempProfileName.append(tr("Tuesday"));
	if (weekday==4)
		tempProfileName.append(tr("Wednesday"));
	if (weekday==5)
		tempProfileName.append(tr("Thursday"));
	if (weekday==6)
		tempProfileName.append(tr("Friday"));
	if (weekday==7)
		tempProfileName.append(tr("Saturday"));

	if (monthday != 0) 
	{
		if (weekday!=0)
			tempProfileName.append(" "+tr("& on the")+" ");

		tempProfileName.append(countStr.setNum(monthday));
		
		if (monthday == 1) tempProfileName.append(tr("st","as in 1st day"));
		if (monthday == 2) tempProfileName.append(tr("nd","as in 2nd day"));
		if (monthday == 3) tempProfileName.append(tr("rd","as in 3rd day"));
		if ((monthday > 3) && (monthday < 8)) tempProfileName.append(tr("th","as in 4th, 5th, 6th or 7th day"));
		if (monthday == 8) tempProfileName.append(tr("th","as in 8th day"));
		if (monthday == 9) tempProfileName.append(tr("th","as in 9th day"));
		if (monthday == 10) tempProfileName.append(tr("th","as in 10th day"));
		if (monthday == 11) tempProfileName.append(tr("th","as in 11th day"));
		if ((monthday > 11) && (monthday < 20)) tempProfileName.append(tr("th","as in 12th....19th day"));
		if (monthday == 20) tempProfileName.append(tr("th","as in 20th day"));
		if (monthday == 21) tempProfileName.append(tr("st","as in 21st day"));
		if (monthday == 22) tempProfileName.append(tr("nd","as in 22nd day"));
		if (monthday == 23) tempProfileName.append(tr("rd","as in 23rd day"));
		if ((monthday > 23) && (monthday < 31)) tempProfileName.append(tr("th","as in 24th...30th day"));
		if (monthday == 31) tempProfileName.append(tr("st","as in 31st day"));
		tempProfileName.append(" "+tr("day"));
	}
	if (month == 0)
	{
		tempProfileName.append(" "+tr("of any month"));
		return tempProfileName;
	}
	if (month == 1)
		tempProfileName.append(" "+tr("of")+" "+tr("January","full phrase: 'of January'"));
	if (month == 2)
		tempProfileName.append(" "+tr("of")+" "+tr("February","full phrase: 'of February'"));
	if (month == 3)
		tempProfileName.append(" "+tr("of")+" "+tr("March","full phrase: 'of March'"));
	if (month == 4)
		tempProfileName.append(" "+tr("of")+" "+tr("April","full phrase: 'of April'"));
	if (month == 5)
		tempProfileName.append(" "+tr("of")+" "+tr("May","full phrase: 'of May'"));
	if (month == 6)
		tempProfileName.append(" "+tr("of")+" "+tr("June","full phrase: 'of June'"));
	if (month == 7)
		tempProfileName.append(" "+tr("of")+" "+tr("July","full phrase: 'of July'"));
	if (month == 8)
		tempProfileName.append(" "+tr("of")+" "+tr("August","full phrase: 'of August'"));
	if (month == 9)
		tempProfileName.append(" "+tr("of")+" "+tr("September","full phrase: 'of September'"));
	if (month == 10)
		tempProfileName.append(" "+tr("of")+" "+tr("October","full phrase: 'of October'"));
	if (month == 11)
		tempProfileName.append(" "+tr("of")+" "+tr("November","full phrase: 'of November'"));
	if (month == 12)
		tempProfileName.append(" "+tr("of")+" "+tr("December","full phrase: 'of December'"));

	return tempProfileName;
}

// createCron=====================================================================================================
//creates a cron file & updates the user's crontab
void scheduleDialog::createCron()
{
	QString ScheduleLine = "";
	QString ProfilePath="";
	QString ProfileName="";
	QString luckyEntryStart = "\n# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ luckybackup entries ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
	QString luckyEntryEnd = "# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ end of luckybackup entries ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

	// copy user's crontab to QString CronTab & delete luckybackup's entries
	CronTab = "";
	cronProcess -> start ("crontab",QStringList() << "-l");	// execute crontab -l command & capture output to QString CronTab
	cronProcess -> waitForStarted();
	cronProcess -> waitForFinished();
	int x = CronTab.indexOf(luckyEntryStart);
	int y = CronTab.indexOf(luckyEntryEnd) + luckyEntryEnd.size() - x;
	CronTab.remove	(x,y);					//remove luckyBackup entries from CronTab

	QFile cronFile (cronfilename);
	if (cronFile.exists())		//if a cronFile exists delete it
		cronFile.remove();

	if (!cronFile.open(QIODevice::WriteOnly | QIODevice::Text))	//create a new cronFile as textfile
		QMessageBox::information(this, appName, tr("Unable to create cron file")+" !!<br><br>"+ cronFile.errorString());
	else 		//write some stuff to schedule file
	{
 		QTextStream out(&cronFile);
		out << CronTab;		//append the old user's crontab first (minus the luckybackup entries)
		out << luckyEntryStart;	//# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ luckybackup entries ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n
		currentSchedule = 0;
		while (currentSchedule < TotalSchedule)
		{
			if (Schedule[currentSchedule] -> GetReboot())			//append @reboot if checkbox selected
            {
				ScheduleLine.append("@reboot    ");
				if (Schedule[currentSchedule] -> GetDelayReboot() > 0 )
                    ScheduleLine.append("sleep " + countStr.setNum(Schedule[currentSchedule] -> GetDelayReboot() * 60) +";    ");
            }
			else									// else append time-day eth
			{
				ScheduleLine.append(countStr.setNum(Schedule[currentSchedule] -> GetMinute()) + " ");	//minute
				ScheduleLine.append(countStr.setNum(Schedule[currentSchedule] -> GetHour()) + " ");	//hour
				if (Schedule[currentSchedule] -> GetMonthDay() == 0)					//Day of month
					ScheduleLine.append("* ");
				else
					ScheduleLine.append(countStr.setNum(Schedule[currentSchedule] -> GetMonthDay()) + " ");
				if (Schedule[currentSchedule] -> GetMonth() == 0)					//Month
					ScheduleLine.append("* ");
				else
					ScheduleLine.append(countStr.setNum(Schedule[currentSchedule] -> GetMonth()) + " ");
				if (Schedule[currentSchedule] -> GetWeekDay() == 0)					//Day of week
					ScheduleLine.append("*	");
				else
					ScheduleLine.append(countStr.setNum(Schedule[currentSchedule] -> GetWeekDay() -1 ) + "	");
			}
			
			if (Schedule[currentSchedule] -> GetConsoleMode())			//append -c --no-questions for CL or --silent for gui
				ScheduleLine.append("/usr/bin/luckybackup -c --no-questions ");
			else
				ScheduleLine.append("env DISPLAY=:0 /usr/bin/luckybackup --silent ");
			
			if (Schedule[currentSchedule] -> GetSkipCritical())			//append --skip-critical
				ScheduleLine.append("--skip-critical ");
			
			ProfilePath = Schedule[currentSchedule] -> GetProfilePath();
			ProfilePath.replace(" ", "\\ ");
			ScheduleLine.append(ProfilePath);	//append profile full path
			
			// append cron logfile part
			ProfileName = Schedule[currentSchedule] -> GetProfileName();
			ProfileName.replace(" ", "\\ ");
			ScheduleLine.append(" > " + logDir + ProfileName + "-" + cronlogString +
					" 2>&1"+ "\n");		//append logfile output with >> instead of single >

			out << ScheduleLine;

			ScheduleLine = "";
			currentSchedule++;
		}
		out << luckyEntryEnd;	//# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ end of luckybackup entries ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n
	}

	cronFile.close();

	cronProcess -> start ("crontab",QStringList() << cronfilename);	// execute crontab command to replace the user's crontab with luckybackup cron
	cronProcess -> waitForStarted();
	cronProcess -> waitForFinished();

	//if the new user's crontab contains no actual schedules > remove user's crontab completely
	CronTab = "";
	cronProcess -> start ("crontab",QStringList() << "-l");	// execute crontab -l command & capture output to QString CronTab
	cronProcess -> waitForStarted();
	cronProcess -> waitForFinished();
	if (CronTab == luckyEntryStart + luckyEntryEnd)
	{
		cronProcess -> start ("crontab",QStringList() << "-r");	// remove user's crontab
		cronProcess -> waitForStarted();
		cronProcess -> waitForFinished();
	}
}

// appendCrontabOutput=====================================================================================================
//appends the user's crontab to a QString
void scheduleDialog::appendCrontabOutput()
{
	QTextCodec *codec = QTextCodec::codecForName("UTF-8");
	CronTab = codec->toUnicode(cronProcess -> readAllStandardOutput());
	//CronTab = QString(cronProcess -> readAllStandardOutput());
}

// end of scheduleDialog.cpp ---------------------------------------------------------------------------

