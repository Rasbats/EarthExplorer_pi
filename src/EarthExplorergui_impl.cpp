/******************************************************************************
*
* Project:  OpenCPN
* Purpose:  EarthExplorer Plugin
* Author:   Mike Rossiter
*
***************************************************************************
*   Copyright (C) 2020 by Mike Rossiter                                   *
*   $EMAIL$                                                               *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************
*/


#include "EarthExplorergui_impl.h"
#include <wx/progdlg.h>
#include <wx/wx.h>
#include "EarthExplorer_pi.h"

#include <stdio.h>
#include <wx/timer.h>
#include "wx/textfile.h"

class GribRecordSet;


void assign(char *dest, char *arrTest2)
{
	strcpy(dest, arrTest2);
}

#define BUFSIZE 0x10000

Dlg::Dlg(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : EarthExplorerBase(parent, id, title, pos, size, style)
{
	this->Fit();
	dbg = false; //for debug output set to true
	initLat = 0;
	initLon = 0;
	m_interval = 500;
	m_bUseSetTime = false;
	m_bUseStop = true;
	m_bUsePause = false;
	m_sNmeaTime = wxEmptyString;

	m_bUsingWind = false;
	m_bUsingFollow = false;
	m_bInvalidPolarsFile = false;
	m_bInvalidGribFile = false;
	m_bEarthExplorerHasStarted = false;

	m_bShowCamera = false;

	wxFileConfig *pConf = GetOCPNConfigObject();

	if (pConf) {
		pConf->SetPath(_T("/Settings/EarthExplorer_pi"));

		pConf->Read(_T("earthexplorerUseAis"), &m_bUseAis, 0);
		pConf->Read(_T("earthexplorerUseFile"), &m_bUseFile, 0);
		pConf->Read(_T("earthexplorerMMSI"), &m_tMMSI, "12345");
		pConf->Read(_T("placemarkFile"), &m_sKmlFile, "");
	}
	

	MyCameraControl = NULL;
}


void Dlg::OnTimer(wxTimerEvent& event){
	Notify();
}

void Dlg::SetNextStep(double inLat, double inLon, double inDir, double inSpd, double &outLat, double &outLon){
	PositionBearingDistanceMercator_Plugin(inLat, inLon, inDir, inSpd, &stepLat, &stepLon);
}

void Dlg::SetFollowStep(double inLat, double inLon, double inDir, double inSpd, double &outLat, double &outLon) {

	double myBrg;

	PositionBearingDistanceMercator_Plugin(inLat, inLon, inDir, inSpd, &stepLat, &stepLon);
	DistanceBearingMercator_Plugin(nextLat, nextLon, stepLat, stepLon, &myBrg, &myDist);

	if (myDist <= initSpd / 7200) {

		//wxString sDist = wxString::Format(_T("%5.2f"), myDist);
		//wxMessageBox(sDist, _T("Dist <15"));
		//return;

		stepLat = nextLat;
		stepLon = nextLon;
		
		nextRoutePointIndex++;

		if (nextRoutePointIndex > (countRoutePoints - 1)) {			
			SetStop();
			return;
		}

		for (std::vector<rtept>::iterator it = routePoints.begin(); it != routePoints.end(); it++) {

			//wxString sIndex = wxString::Format(_T("%i"), (*it).index);
			//wxMessageBox(sIndex, _T("index"));			

			double value;

			if ((*it).index == nextRoutePointIndex) {

				(*it).lat.ToDouble(&value);
				nextLat = value;

				(*it).lon.ToDouble(&value);
				nextLon = value;
			}
		}
		DistanceBearingMercator_Plugin(nextLat, nextLon, stepLat, stepLon, &followDir, &myDist);
		PositionBearingDistanceMercator_Plugin(stepLat, stepLon, followDir, inSpd, &stepLat, &stepLon);
		myDir = followDir;
	}	
}

void Dlg::OnStart(wxCommandEvent& event) {

	StartDriving();
}


wxString Dlg::SetFileKML(wxString kmlFile){

	wxString kmlOut;
	wxString stdPath(*GetpPrivateApplicationDataLocation());
	wxString s = wxFileName::GetPathSeparator();

	stdPath = stdPath + s + _T("plugins") + s + _T("EarthExplorer_pi");
	if (!wxDirExists(stdPath))
		wxMkdir(stdPath);
	    stdPath += s + _T("data");
	if (!wxDirExists(stdPath))
		wxMkdir(stdPath);
	kmlOut = stdPath + s + kmlFile;

	return kmlOut;
}

wxString Dlg::GetModelFile(wxString modelFile) {

	wxString locn = *GetpSharedDataLocation();
	wxString s = wxFileName::GetPathSeparator();

	wxString modelLocn = locn + s + "plugins" + s + "EarthExplorer_pi" + s + "data" + s;
	wxString myModel = modelLocn + modelFile;
	return myModel;
}

void Dlg::StartDriving() {

	m_sKmlFile = SetFileKML("placemark.kml");

	if (initLat == 0.0){
		wxMessageBox(_("Please right-click and choose vessel start position"));
		return;
	}

	m_bEarthExplorerHasStarted = true;
	m_bUsingWind = false;

	if (!m_tMMSI.ToLong(&m_iMMSI)) {
		wxMessageBox(_("MMSI must be a number, please change in Preferences"));
		return;
	}

	if (m_bUseFile){

		wxString caption = wxT("Choose a file");
		wxString wildcard = wxT("Text files (*.txt)|*.txt|All files (*.*)|*.*");

		wxString s = _T("/");
		char * pName = "EarthExplorer_pi";
		wxString defaultDir = GetPluginDataDir(pName) + s + _T("data") + s;

		wxString defaultFilename = wxEmptyString;
		wxFileDialog filedlg(this->m_parent, caption, defaultDir, defaultFilename, wildcard, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);


		if (filedlg.ShowModal() != wxID_OK)
		{
			wxMessageBox(_("EarthExplorer has been stopped"));
			return;
		}
		else{
			nmeafile = new wxTextFile(filedlg.GetPath());
			nmeafile->Open();
			nmeafile->Clear();
		}
	}

	m_textCtrlRudderStbd->SetValue(_T(""));
	m_textCtrlRudderPort->SetValue(_T(""));
	initSpd = 0; // 5 knots

	if (!m_bUsingFollow) {

		wxString myHeading = m_stHeading->GetLabel();
		myHeading.ToDouble(&initDir);
		myDir = initDir;

	}
	else {
		myDir = followDir;
	}

	dt = dt.Now();
	GLL = createGLLSentence(dt, initLat, initLon, initSpd / 7200, initDir);
	VTG = createVTGSentence(initSpd, initDir);
	

	m_interval = 500;
	m_Timer->Start(m_interval, wxTIMER_CONTINUOUS); // start timer
	m_bAuto = false;

	myAIS = new AisMaker();

}

void Dlg::OnStop(wxCommandEvent& event) {

	SetStop();

}


void Dlg::SetStop() {

	

	if (m_Timer->IsRunning()) m_Timer->Stop();

	if (m_bUsingFollow) {
		wxMessageBox(_T("Vessel is stopping"));
	}

	m_SliderSpeed->SetValue(0);
	m_SliderRudder->SetValue(30);
	m_textCtrlRudderStbd->SetValue(_T(""));
	m_textCtrlRudderPort->SetValue(_T(""));

	m_interval = m_Timer->GetInterval();
	m_bUseSetTime = false;
	m_bUseStop = true;
	m_bAuto = false;
	m_bUsingWind = false;
	m_bUsingFollow = false;

	m_buttonWind->SetBackgroundColour(wxColour(0, 255, 0));

	if (m_bUseFile){
		nmeafile->Write();
		nmeafile->Close();
	}
	initSpd = 0.0;
	m_stSpeed->SetLabel(wxString::Format(_T("%3.1f"), initSpd));

}

void Dlg::OnMidships(wxCommandEvent& event){
	m_SliderRudder->SetValue(30);
}

void Dlg::OnMinus10(wxCommandEvent& event){
	
	m_bAuto = false;
	GoToStandby();
	myDir -= 10;
	wxString mystring = wxString::Format(wxT("%03.0f"), myDir);
	m_stHeading->SetLabel(mystring);
	
}

void Dlg::OnPlus10(wxCommandEvent& event){
	
	m_bAuto = false;
	GoToStandby();
	myDir += 10;
	wxString mystring = wxString::Format(wxT("%03.0f"), myDir);
	m_stHeading->SetLabel(mystring);
	
}

void Dlg::OnMinus1(wxCommandEvent& event){
	
	m_bAuto = false;
	GoToStandby();
	myDir -= 1;
	wxString mystring = wxString::Format(wxT("%03.0f"), myDir);
	m_stHeading->SetLabel(mystring);
	
}

void Dlg::OnPlus1(wxCommandEvent& event){
	
	m_bAuto = false;
	GoToStandby();
	myDir += 1;
	wxString mystring = wxString::Format(wxT("%03.0f"), myDir);
	m_stHeading->SetLabel(mystring);
	
}

void Dlg::OnAuto(wxCommandEvent& event){

	m_bAuto = true;

	m_buttonStandby->SetBackgroundColour(wxColour(255, 0, 0));
	m_buttonAuto->SetBackgroundColour(wxColour(0, 255, 0));
	Refresh();
}

void Dlg::OnStandby(wxCommandEvent& event){

	GoToStandby();
}

void Dlg::GoToStandby() {

	m_bAuto = false;

	m_buttonStandby->SetBackgroundColour(wxColour(0, 255, 0));
	m_buttonAuto->SetBackgroundColour(wxColour(255, 255, 255));
	Refresh();

}

void Dlg::OnClose(wxCloseEvent& event)
{
	if (m_Timer->IsRunning()) m_Timer->Stop();
	if (MyCameraControl != NULL) {
		MyCameraControl->Close();
	}
	plugin->OnEarthExplorerDialogClose();
	plugin->m_sCopyKmlFile = m_sKmlFile;
}
void Dlg::Notify()
{
	WriteKML();
	wxString mySentence;
	plugin->SetNMEASentence(mySentence);
	

	initSpd = m_SliderSpeed->GetValue();
	initRudder = m_SliderRudder->GetValue();

	double myRudder = initRudder - 30;
	if (myRudder < 0){
		initRudder -= 30.0;
		myRudder = std::abs(initRudder);
		myDir -= myRudder;
		double myPortRudder = 30 - std::abs(myRudder);
		m_gaugeRudderPort->SetValue(myPortRudder);
		m_textCtrlRudderPort->SetValue(wxString::Format(_T("%.0f"), myRudder) + _T(" P"));
		m_gaugeRudderStbd->SetValue(0);
		m_textCtrlRudderStbd->SetValue(_T(""));
	}
	else if (myRudder >= 0){

		initRudder -= 30;
		myDir += initRudder;
		m_gaugeRudderStbd->SetValue(myRudder);
		if (myRudder == 0){
			m_textCtrlRudderStbd->SetValue(_T(""));
		}
		else {
			m_textCtrlRudderStbd->SetValue(wxString::Format(_T("%.0f"), myRudder) + _T(" S"));
		}
		m_gaugeRudderPort->SetValue(0);
		m_textCtrlRudderPort->SetValue(_T(""));

	}

	if (myDir < 0){
		myDir += 360;
	}
	else if (myDir > 360){
		myDir -= 360;
	}

	wxString mystring = wxString::Format(wxT("%03.0f"), myDir);
	m_stHeading->SetLabel(mystring);

	if (m_bUsingWind){
		double polarBoatSpeed = GetPolarSpeed(initLat, initLon, myDir);
		if (polarBoatSpeed != -1){
			initSpd = polarBoatSpeed;
		}
	}

	m_stSpeed->SetLabel(wxString::Format(_T("%3.1f"), initSpd));

	if (!m_bUsingFollow) {
		
		SetNextStep(initLat, initLon, myDir, initSpd / 7200, stepLat, stepLon);
		
	}
	else {
		SetFollowStep(initLat, initLon, myDir, initSpd / 7200, stepLat, stepLon);
	}

	
	wxString timeStamp = wxString::Format(_T("%i"), wxGetUTCTime());

	wxString myNMEAais = myAIS->nmeaEncode(_T("18"), m_iMMSI, _T("5"), initSpd, initLat, initLon, myDir, myDir, _T("B"), timeStamp);

	if (m_bUseFile)	nmeafile->AddLine(myNMEAais);

	int ss = 1;
	wxTimeSpan mySeconds = wxTimeSpan::Seconds(ss);
	wxDateTime mdt = dt.Add(mySeconds);

    bool m_bGrib;
	double wspd, wdir;
	if (m_bUsingWind) {
		m_bGrib = GetGribSpdDir(dt, initLat, initLon, wspd, wdir);
	}

	if (m_bGrib && m_bUsingWind){
		MWVA = createMWVASentence(initSpd, myDir, wdir, wspd);
		MWVT = createMWVTSentence(initSpd, myDir, wdir, wspd);
		MWD = createMWDSentence(wdir, wspd);
			
		PushNMEABuffer(MWVA + _T("\n"));
		PushNMEABuffer(MWVT + _T("\n"));
		PushNMEABuffer(MWD + _T("\n"));
		
	}

	

	GLL = createGLLSentence(mdt, initLat, initLon, initSpd, myDir);
	VTG = createVTGSentence(initSpd, myDir);
	VHW = createVHWSentence(initSpd, myDir);
	

	PushNMEABuffer(GLL + _T("\n"));
	PushNMEABuffer(VTG + _T("\n"));
	PushNMEABuffer(VHW + _T("\n"));
	

	if (m_bUseAis) PushNMEABuffer(myNMEAais + _T("\n"));

	initLat = stepLat;
	initLon = stepLon;

	dt = mdt;

}

void Dlg::SetInterval(int interval)
{
	m_interval = interval;
	if (m_Timer->IsRunning()) // Timer started?
		m_Timer->Start(m_interval, wxTIMER_CONTINUOUS); // restart timer with new interval
}

wxString Dlg::createMWDSentence(double winddirection, double windspeed){
	/*
	1    2   3   4        5
	|    |   |   |        |
	$--MWV, x.x, a, x.x, a*hh

	Field Number :
	1. Wind Angle, 0 to 360 degrees
	2.Reference, R = Relative, T = True
	3.Wind Speed
	4.Wind Speed Units, K / M / N
	5.Status, A = Data Valid
	Checksum
	*/

	/*
	+     * $WIMWD,<1>,<2>,<3>,<4>,<5>,<6>,<7>,<8>*hh
	+     *
	+     * NMEA 0183 standard Wind Direction and Speed, with respect to north.
	+     *
	+     * <1> Wind direction, 0.0 to 359.9 degrees True, to the nearest 0.1 degree
	+     * <2> T = True
	+     * <3> Wind direction, 0.0 to 359.9 degrees Magnetic, to the nearest 0.1
	degree
	+     * <4> M = Magnetic
	+     * <5> Wind speed, knots, to the nearest 0.1 knot.
	+     * <6> N = Knots
	+     * <7> Wind speed, meters/second, to the nearest 0.1 m/s.
	+     * <8> M = Meters/second
	+     */

	wxString nMWV;
	wxString nMWD;
	wxString nDir;
	wxString nRelTrue;
	wxString nSpd;
	wxString nValid;
	wxString nForCheckSum;
	wxString nFinal;
	wxString nUnits;
	wxString nC = _T(",");
	wxString nA = _T("A");
	nUnits = _T("N");
	nMWV = _T("WIMWV");
	nMWD = _T("WIMWD");
	nRelTrue = _T("T");
	nValid = _T("A,A");
	wxString ndlr = _T("$");
	wxString nast = _T("*");

	nSpd = wxString::Format(_T("%f"), windspeed);
	nDir = wxString::Format(_T("%f"), winddirection);

	//nForCheckSum = nMWV + nC + nDir + nC + nRelTrue + nC + nSpd + nC + nUnits + nC + nA;
	nForCheckSum = nMWD + nC + nDir + nC + nRelTrue + nC  + nC + nC + nSpd + nC + nUnits + nC + nC ;
	nFinal = ndlr + nForCheckSum + nast + makeCheckSum(nForCheckSum);
	return nFinal;

}

wxString Dlg::createVHWSentence(double stw, double hdg){
	/*
	VHW Water Speed and Heading
	1   2   3   4   5   6   7   8 9
	|   |   |   |   |   |   |   | |
	$--VHW, x.x, T, x.x, M, x.x, N, x.x, K*hh
	1) Degress	True
	2) T = True
	3) Degrees Magnetic
	4) M = Magnetic
	5) Knots(speed of vessel relative to the water)
	6) N = Knots
	7) Kilometers (speed of vessel relative to the water)
	8) K = Kilometres
	9) Checksum
	*/
	wxString nVHW;
	wxString nDir;
	wxString nTrueMag;
	wxString nSpd;
	wxString nValid;
	wxString nForCheckSum;
	wxString nFinal;
	wxString nUnits;
	wxString nC = _T(",");
	wxString nA = _T("A");
	nUnits = _T("N");
	nVHW = _T("IIVHW");
	nTrueMag = _T("T");
	wxString ndlr = _T("$");
	wxString nast = _T("*");

	nSpd = wxString::Format(_T("%f"), stw);
	nDir = wxString::Format(_T("%f"), hdg);

	nForCheckSum = nVHW + nC + nDir + nC + nTrueMag + nC + nC + nC + nSpd + nC + nUnits;
	nFinal = ndlr + nForCheckSum + nast + makeCheckSum(nForCheckSum);
	return nFinal;

}

wxString Dlg::createMWVTSentence(double spd, double hdg, double winddirection, double windspeed){
	/*
	1    2   3   4        5
	|    |   |   |        |
	$--MWV, x.x, a, x.x, a*hh

	Field Number :
	1. Wind Angle, 0 to 360 degrees
	2.Reference, R = Relative, T = True (theoretical)
	3.Wind Speed
	4.Wind Speed Units, K / M / N
	5.Status, A = Data Valid
	Checksum
	*/

	/*
	+     * $WIMWD,<1>,<2>,<3>,<4>,<5>,<6>,<7>,<8>*hh
	+     *
	+     * NMEA 0183 standard Wind Direction and Speed, with respect to north.
	+     *
	+     * <1> Wind direction, 0.0 to 359.9 degrees True, to the nearest 0.1 degree
	+     * <2> T = True
	+     * <3> Wind direction, 0.0 to 359.9 degrees Magnetic, to the nearest 0.1
	degree
	+     * <4> M = Magnetic
	+     * <5> Wind speed, knots, to the nearest 0.1 knot.
	+     * <6> N = Knots
	+     * <7> Wind speed, meters/second, to the nearest 0.1 m/s.
	+     * <8> M = Meters/second
	+
	*/

	double twa = 360 - ((hdg - winddirection) - 360);
	if (twa > 360){
		twa -= 360;
		if (twa > 360){
			twa -= 360;
		}
	}

	double tws = windspeed;

	wxString nMWV;
	wxString nMWD;
	wxString nDir;
	wxString nRelTrue;
	wxString nSpd;
	wxString nValid;
	wxString nForCheckSum;
	wxString nFinal;
	wxString nUnits;
	wxString nC = _T(",");
	wxString nA = _T("A");
	nUnits = _T("N");
	nMWV = _T("WIMWV");
	nMWD = _T("WIMWD");
	nRelTrue = _T("T");
	nValid = _T("A,A");
	wxString ndlr = _T("$");
	wxString nast = _T("*");

	nSpd = wxString::Format(_T("%f"), tws);
	nDir = wxString::Format(_T("%f"), twa);

	nForCheckSum = nMWV + nC + nDir + nC + nRelTrue + nC + nSpd + nC + nUnits + nC + nA;
	//$--MWD, x.x, T, x.x, M, x.x, N, x.x, M*hh<CR><LF>
	//MWD,270.7,T,,,20.5,N,,
	//nForCheckSum = nMWD + nC + nDir + nC + nRelTrue + nC  + nC + nC + nSpd + nC + nUnits + nC + nC ;
	nFinal = ndlr + nForCheckSum + nast + makeCheckSum(nForCheckSum);
	return nFinal;

}

wxString Dlg::createMWVASentence(double spd, double hdg, double winddirection, double windspeed){
	/*
	1    2   3   4        5
	|    |   |   |        |
	$--MWV, x.x, a, x.x, a*hh

	Field Number :
	1. Wind Angle, 0 to 360 degrees
	2.Reference, R = Relative, T = True (theoretical)
	3.Wind Speed
	4.Wind Speed Units, K / M / N
	5.Status, A = Data Valid
	Checksum
	*/

	/*
	+     * $WIMWD,<1>,<2>,<3>,<4>,<5>,<6>,<7>,<8>*hh
	+     *
	+     * NMEA 0183 standard Wind Direction and Speed, with respect to north.
	+     *
	+     * <1> Wind direction, 0.0 to 359.9 degrees True, to the nearest 0.1 degree
	+     * <2> T = True
	+     * <3> Wind direction, 0.0 to 359.9 degrees Magnetic, to the nearest 0.1
	degree
	+     * <4> M = Magnetic
	+     * <5> Wind speed, knots, to the nearest 0.1 knot.
	+     * <6> N = Knots
	+     * <7> Wind speed, meters/second, to the nearest 0.1 m/s.
	+     * <8> M = Meters/second
	+
	*/

	double twa = 360 - ((hdg - winddirection) - 360);
	if (twa > 360){
		twa -= 360;
		if (twa > 360){
			twa -= 360;
		}
	}
	wxString leftright = wxEmptyString;

	if (twa <= 180){
		leftright = _T("R");
	}
	if (twa > 180){
		leftright = _T("L");
		twa = 360 - twa;
	}

	double aws, twd, tws, awd, awa;
	twa = 180 - twa;  // we need the complement of the twa for the internal angle of the triangle
	twa = twa * M_PI / 180; // convert to radians
	tws = windspeed;
	double alpha, bravo, charlie, delta;
	alpha = pow(spd, 2) + pow(windspeed, 2) - 2 * spd*windspeed*cos(twa);
	aws = sqrt(alpha);

	//spd / charlie = aws / twa;

	charlie = spd * sin(twa) / aws;
	charlie = asin(charlie);
	twa = M_PI - twa;  // complement in radians
	awa = twa - charlie;
	awa = awa * 180 / M_PI; // back to degrees

	if (leftright == _T("L")) {
		awa = 360 - awa;
	}

	wxString nMWV;
	wxString nMWD;
	wxString nDir;
	wxString nRelTrue;
	wxString nSpd;
	wxString nValid;
	wxString nForCheckSum;
	wxString nFinal;
	wxString nUnits;
	wxString nC = _T(",");
	wxString nA = _T("A");
	nUnits = _T("N");
	nMWV = _T("WIMWV");
	nMWD = _T("WIMWD");
	nRelTrue = _T("R");
	nValid = _T("A,A");
	wxString ndlr = _T("$");
	wxString nast = _T("*");

	nSpd = wxString::Format(_T("%f"), aws);
	nDir = wxString::Format(_T("%f"), awa);

	nForCheckSum = nMWV + nC + nDir + nC + nRelTrue + nC + nSpd + nC + nUnits + nC + nA;
	//$--MWD, x.x, T, x.x, M, x.x, N, x.x, M*hh<CR><LF>
	//MWD,270.7,T,,,20.5,N,,
	//nForCheckSum = nMWD + nC + nDir + nC + nRelTrue + nC  + nC + nC + nSpd + nC + nUnits + nC + nC ;
	nFinal = ndlr + nForCheckSum + nast + makeCheckSum(nForCheckSum);
	return nFinal;

}
wxString Dlg::createRMCSentence(wxDateTime myDateTime, double myLat, double myLon, double mySpd, double myDir){
	//$GPRMC, 110858.989, A, 4549.9135, N, 00612.2671, E, 003.7, 207.5, 050513, , , A * 60
	//$GPRMC,110858.989,A,4549.9135,N,00612.2671,E,003.7,207.5,050513,,,A*60

	wxString nlat;
	wxString nlon;
	wxString nRMC;
	wxString nNS;
	wxString nEW;
	wxString nSpd;
	wxString nDir;
	wxString nTime;
	wxString nDate;
	wxString nValid;
	wxString nForCheckSum;
	wxString nFinal;
	wxString nC = _T(",");
	wxString nA = _T("A,");
	nRMC = _T("GPRMC,");
	nValid = _T("A,A");
	wxString ndlr = _T("$");
	wxString nast = _T("*");

	nTime = DateTimeToTimeString(myDateTime);
	nNS = LatitudeToString(myLat);
	nEW = LongitudeToString(myLon);
	nSpd = wxString::Format(_T("%f"), mySpd);
	nDir = wxString::Format(_T("%f"), myDir);
	nDate = DateTimeToDateString(myDateTime);

	nForCheckSum = nRMC + nTime + nC + nNS + nEW + nSpd + nC + nDir + nC + nDate + _T(",,,A");
	nFinal = ndlr + nForCheckSum + nast + makeCheckSum(nForCheckSum);
	return nFinal;
}

wxString Dlg::createGLLSentence(wxDateTime myDateTime, double myLat, double myLon, double mySpd, double myDir){

	//$IIGLL,5027.776667,N,412.690754,W,123327,A*26

	wxString nlat;
	wxString nlon;
	wxString nGLL;
	wxString nNS;
	wxString nEW;
	wxString nSpd;
	wxString nDir;
	wxString nTime;
	wxString nDate;
	wxString nValid;
	wxString nForCheckSum;
	wxString nFinal;
	wxString nC = _T(",");
	wxString nA = _T("A,");
	nGLL = _T("IIGLL,");
	nValid = _T("A,A");
	wxString ndlr = _T("$");
	wxString nast = _T("*");

	nTime = DateTimeToTimeString(myDateTime);
	nNS = LatitudeToString(myLat);
	nEW = LongitudeToString(myLon);
	nSpd = wxString::Format(_T("%f"), mySpd);
	nDir = wxString::Format(_T("%f"), myDir);
	nDate = DateTimeToDateString(myDateTime);

	nForCheckSum = nGLL + nNS + nEW + nTime + _T(",A");
	nFinal = ndlr + nForCheckSum + nast + makeCheckSum(nForCheckSum);
	//wxMessageBox(nFinal);
	return nFinal;
}

wxString Dlg::createVTGSentence(double mySpd, double myDir){
	//$GPVTG, 054.7, T, 034.4, M, 005.5, N, 010.2, K * 48
	//$IIVTG, 307., T, , M, 08.5, N, 15.8, K, A * 2F
	wxString nSpd;
	wxString nDir;
	wxString nTime;
	wxString nDate;
	wxString nValid;
	wxString nForCheckSum;
	wxString nFinal;
	wxString nC = _T(",");
	wxString nA = _T("A");
	wxString nT = _T("T,");
	wxString nM = _T("M,");
	wxString nN = _T("N,");
	wxString nK = _T("K,");

	wxString nVTG = _T("IIVTG,");
	nValid = _T("A,A");
	wxString ndlr = _T("$");
	wxString nast = _T("*");

	nSpd = wxString::Format(_T("%f"), mySpd);
	nDir = wxString::Format(_T("%f"), myDir);

	nForCheckSum = nVTG + nDir + nC + nT + nC + nM + nSpd + nN + nC + nC + nA;

	nFinal = ndlr + nForCheckSum + nast + makeCheckSum(nForCheckSum);
	//wxMessageBox(nFinal);
	return nFinal;
}

wxString Dlg::createHDTSentence(double myDir){
	/*
	1   2 3
	|   | |
	$--HDT, x.x, T*hh<CR><LF>
	*/
	wxString nSpd;
	wxString nDir;
	wxString nTime;
	wxString nDate;
	wxString nValid;
	wxString nForCheckSum;
	wxString nFinal;
	wxString nC = _T(",");
	wxString nA = _T("A");
	wxString nT = _T("T,");
	wxString nM = _T("M,");
	wxString nN = _T("N,");
	wxString nK = _T("K,");

	wxString nHDG = _T("IIHDT,");
	nValid = _T("A,A");
	wxString ndlr = _T("$");
	wxString nast = _T("*");

	nDir = wxString::Format(_T("%f"), myDir);

	nForCheckSum = nHDG + nDir + nC + nT;

	nFinal = ndlr + nForCheckSum + nast + makeCheckSum(nForCheckSum);
	//wxMessageBox(nFinal);
	return nFinal;
}


wxString Dlg::makeCheckSum(wxString mySentence){
	int i;
	unsigned char XOR;

	wxString s(mySentence);
	wxCharBuffer buffer = s.ToUTF8();
	char *Buff = buffer.data();	// data() returns const char *
	unsigned long iLen = strlen(Buff);
	for (XOR = 0, i = 0; i < iLen; i++)
		XOR ^= (unsigned char)Buff[i];
	stringstream tmpss;
	tmpss << hex << (int)XOR << endl;
	wxString mystr = tmpss.str();
	return mystr;
}

double StringToLatitude(wxString mLat) {

	//495054
	double returnLat;
	wxString mBitLat = mLat(0, 2);
	double degLat;
	mBitLat.ToDouble(&degLat);
	wxString mDecLat = mLat(2, mLat.length());
	double decValue;
	mDecLat.ToDouble(&decValue);

	returnLat = degLat + decValue / 100 / 60;

	return returnLat;
}

wxString Dlg::LatitudeToString(double mLat) {

	wxString singlezero = _T("0");
	wxString mDegLat;

	int degLat = std::abs(mLat);
	wxString finalDegLat = wxString::Format(_T("%i"), degLat);

	int myL = finalDegLat.length();
	switch (myL){
	case(1) : {
		mDegLat = singlezero + finalDegLat;
		break;
	}
	case(2) : {
		mDegLat = finalDegLat;
		break;
	}
	}

	double minLat = std::abs(mLat) - degLat;
	double decLat = minLat * 60;

	wxString returnLat;
	//wxMessageBox(returnLat, _T("returnLat"));

	if (mLat >= 0){
		if (decLat < 10){
			returnLat = mDegLat + _T("0") + wxString::Format(_T("%.6f"), decLat) + _T(",N,");
		}
		else {
			returnLat = mDegLat + wxString::Format(_T("%.6f"), decLat) + _T(",N,");
		}

	}
	else if (mLat < 0) {
		if (decLat < 10){
			returnLat = mDegLat + _T("0") + wxString::Format(_T("%.6f"), decLat) + _T(",S,");
		}
		else {
			returnLat = mDegLat + wxString::Format(_T("%.6f"), decLat) + _T(",S,");
		}
	}

	return returnLat;
}
double StringToLongitude(wxString mLon) {

	wxString mBitLon = "";
	wxString mDecLon;
	double value1;
	double decValue1;

	double returnLon;

	int m_len = mLon.length();

	if (m_len == 7)
	{
		mBitLon = mLon(0, 3);
	}

	if (m_len == 6)
	{
		mBitLon = mLon(0, 2);
	}

	if (m_len == 5)
	{
		mBitLon = mLon(0, 1);
	}

	if (m_len == 4)
	{
		mBitLon = "00.00";
	}

	if (mBitLon == "-")
	{
		value1 = -0.00001;
	}
	else
	{
		mBitLon.ToDouble(&value1);
	}

	mDecLon = mLon(mLon.length() - 4, mLon.length());
	mDecLon.ToDouble(&decValue1);

	if (value1 < 0)
	{
		returnLon = value1 - decValue1 / 100 / 60;
	}
	else
	{
		returnLon = value1 + decValue1 / 100 / 60;
	}

	return returnLon;
}

wxString Dlg::LongitudeToString(double mLon) {

	wxString mDecLon;
	wxString mDegLon;
	double decValue;
	wxString returnLon;
	wxString doublezero = _T("00");
	wxString singlezero = _T("0");

	int degLon = fabs(mLon);
	wxString inLon = wxString::Format(_T("%i"), degLon);

	//wxMessageBox(returnLon, _T("inLon"));

	int myL = inLon.length();
	switch (myL){
	case(1) : {
		mDegLon = doublezero + inLon;
		break;
	}
	case(2) : {
		mDegLon = singlezero + inLon;
		break;
	}
	case(3) : {
		mDegLon = inLon;
		break;
	}
	}
	decValue = std::abs(mLon) - degLon;
	double decLon = decValue * 60;

	if (mLon >= 0){
		if (decLon < 10){
			returnLon = mDegLon + _T("0") + wxString::Format(_T("%.6f"), decLon) + _T(",E,");
		}
		else {
			returnLon = mDegLon + wxString::Format(_T("%.6f"), decLon) + _T(",E,");
		}

	}
	else  {
		if (decLon < 10){
			returnLon = mDegLon + _T("0") + wxString::Format(_T("%.6f"), decLon) + _T(",W,");
		}
		else {
			returnLon = mDegLon + wxString::Format(_T("%.6f"), decLon) + _T(",W,");
		}
	}
	//wxMessageBox(returnLon, _T("returnLon"));
	return returnLon;
}

wxString Dlg::DateTimeToTimeString(wxDateTime myDT) {
	wxString sHours, sMinutes, sSecs;
	sHours = myDT.Format(_T("%H"));
	sMinutes = myDT.Format(_T("%M"));
	sSecs = myDT.Format(_T("%S"));
	wxString dtss = sHours + sMinutes + sSecs;
	return dtss;
}

wxString Dlg::DateTimeToDateString(wxDateTime myDT) {

	wxString sDay, sMonth, sYear;
	sDay = myDT.Format(_T("%d"));
	sMonth = myDT.Format(_T("%m"));
	sYear = myDT.Format(_T("%y"));

	return sDay + sMonth + sYear;
}

void Dlg::OnContextMenu(double m_lat, double m_lon){

	m_buttonWind->SetBackgroundColour(wxColour(0, 255, 0));
	m_bUsingWind = false;

	initLat = m_lat;
	initLon = m_lon;
}

void Dlg::RequestGrib(wxDateTime time){

	Json::Value value;
	time = time.FromUTC();

	value["Day"] = time.GetDay();
	value["Month"] = time.GetMonth();
	value["Year"] = time.GetYear();
	value["Hour"] = time.GetHour();
	value["Minute"] = time.GetMinute();
	value["Second"] = time.GetSecond();

	wxString out;
    
    Json::FastWriter writer;
    out = writer.write(value);
        

	SendPluginMessage(wxString(_T("GRIB_TIMELINE_RECORD_REQUEST")), out);

	Lock();
	m_bNeedsGrib = false;
	Unlock();
}

bool Dlg::GetGribSpdDir(wxDateTime dt, double lat, double lon, double &spd, double &dir){

	wxDateTime dtime = dt;

	plugin->m_grib_lat = lat;
	plugin->m_grib_lon = lon;
	RequestGrib(dtime);
	if (plugin->m_bGribValid){
		spd = plugin->m_tr_spd;
		dir = plugin->m_tr_dir;
		return true;
	}
	else {
		return false;
	}

}

void Dlg::OnWind(wxCommandEvent& event){

	m_bUsingFollow = false;

	if (initLat == 0.0){
		wxMessageBox(_("Please right-click and choose vessel start position"));
		return;
	}
	if (!m_bEarthExplorerHasStarted){
		wxMessageBox(_("Please start EarthExplorer"));
		return;
	}

	m_SliderSpeed->SetValue(0);
	double scale_factor = GetOCPNGUIToolScaleFactor_PlugIn();
	JumpToPosition(initLat, initLon, scale_factor);

	if (!m_bUsingWind){
		m_buttonWind->SetBackgroundColour(wxColour(255, 0, 0));
		m_bUsingWind = true;
		double myPolarSpeed = GetPolarSpeed(initLat, initLon, initDir);
		if (myPolarSpeed == -1){
			if (m_bInvalidPolarsFile){
				wxMessageBox(_T("Invalid Boat Polars file"));
			}

			if (m_bInvalidGribFile){
				wxMessageBox(_T("Grib data is not available for the present date/time or location"));
			}
			m_buttonWind->SetBackgroundColour(wxColour(0, 255, 0));
			m_bUsingWind = false;
		}
	}
	else {
		m_buttonWind->SetBackgroundColour(wxColour(0, 255, 0));
		m_bUsingWind = false;
		
	}
}

double Dlg::GetPolarSpeed(double lat, double lon, double cse){
	double lati = lat;
	double loni = lon;
	double spd;
	double dir;

	wxDateTime dt;
	dt = wxDateTime::UNow();

	bool m_bGrib = GetGribSpdDir(dt, lati, loni, spd, dir);
	if (!m_bGrib){
		m_bInvalidGribFile = true;
		return -1;
	}

	wxString error;
	wxString s = _T("/");

	char * pName = "EarthExplorer_pi";	

	wxString polars_path = GetPluginDataDir(pName) + s + _T("data") + s;
	wxString myFile = polars_path + _T("arcona.xml");

	double twa = 360 - ((cse - dir) - 360);
	if (twa > 360){
		twa -= 360;
		if (twa > 360){
			twa -= 360;
		}
	}

	if (twa > 180){
		twa = 360 - twa;
	}
	/*
	double relWind = cse - dir;

	relWind = abs(relWind);
	if (relWind > 180){
	relWind = 360 - relWind;
	}
	*/
	//wxMessageBox(wxString::Format(_T("%f"), relWind));

	double polarSpeed = ReadPolars(myFile, twa, spd);
	return polarSpeed;
}

double Dlg::ReadPolars(wxString filename, double windangle, double windspeed){

	bool foundWindAngle = false;
	bool foundWindSpeed = false;
	bool foundPreviousWindAngle = false;

	double myWindAngle = -1;
	double myWindSpeed = -1;
	double prevAngle = -1;
	double prevSpeed = -1;
	double dSpeed = -1;
	double prevPolarSpeed = -1;
	wxString myPolarSpeed;

	wxString theWindAngle;

	TiXmlDocument doc;
	wxString error;

	wxFileName fn(filename);

	if (!doc.LoadFile(filename.mb_str())){
		m_bInvalidPolarsFile = true;
		return -1;
	}
	else {
		TiXmlHandle root(doc.RootElement());

		if (strcmp(root.Element()->Value(), "EarthExplorer")){
			m_bInvalidPolarsFile = true;
			return -1;
		}

		int count = 0;
		for (TiXmlElement* e = root.FirstChild().Element(); e; e = e->NextSiblingElement())
			count++;

		int i = 0;
		for (TiXmlElement* e = root.FirstChild().Element(); e; e = e->NextSiblingElement(), i++) {

			if (!strcmp(e->Value(), "TWA") && windangle > myWindAngle && !foundWindAngle && !foundWindSpeed) {
				myWindAngle = AttributeDouble(e, "WindAngle", NAN);
				if (prevAngle < windangle && windangle < myWindAngle){
					theWindAngle = wxString::Format(_T("%5.2f"), prevAngle);
					foundWindAngle = true;
					break;
				}
				prevAngle = myWindAngle;
			}
		}

		if (foundWindAngle){

			// we have found the polar for the next highest wind speed
			// need to move back to the previous polar ... given by windAngle

			TiXmlElement* e;
			for (e = root.FirstChild().Element(); e; e = e->NextSiblingElement(), i++) {

				if (!strcmp(e->Value(), "TWA")) {
					myWindAngle = AttributeDouble(e, "WindAngle", NAN);
					wxString angleOut = wxString::Format(_T("%5.2f"), myWindAngle);
					if (angleOut == theWindAngle){  // we have found the correct section of the polars file	for the relative wind													
						for (TiXmlElement* g = e->FirstChildElement(); g; g = g->NextSiblingElement()) {

							if (!strcmp(g->Value(), "SPD") && windspeed > myWindSpeed) {
								myWindSpeed = AttributeDouble(g, "WindSpeed", NAN);
								wxString myPolarSpeed = g->GetText();
								double dSpeed;
								myPolarSpeed.ToDouble(&dSpeed);

								if (prevSpeed < windspeed && windspeed < myWindSpeed){
									//wxString boatSpeed = wxString::Format(_T("%5.2f"), prevPolarSpeed);
									return prevPolarSpeed;
								}

								prevSpeed = myWindSpeed;// attribute for wind speed
								prevPolarSpeed = dSpeed; // value for boat speed

							}
						}
					}
				}
			}

		}

	}

	m_bInvalidPolarsFile = true;
	return -1;

}

double Dlg::AttributeDouble(TiXmlElement *e, const char *name, double def)
{
	const char *attr = e->Attribute(name);
	if (!attr)
		return def;
	char *end;
	double d = strtod(attr, &end);
	if (end == attr)
		return def;
	return d;
}

double Dlg::ReadNavobj() {

	rte myRte;
	rtept myRtePt;
	vector<rtept> my_points;

	my_routes.clear();		
	
	
	wxString rte_lat;
	wxString rte_lon;

	wxString wpt_guid;

	wxString navobj_path = Dlg::StandardPath();
	wxString myFile = navobj_path + _T("navobj.xml");

	//wxMessageBox(myFile);

	TiXmlDocument doc;
	wxString error;	

	if (!doc.LoadFile(myFile.mb_str())) {
		wxMessageBox(_T("Unable to read navobj file"));
		return -1;
	}
	else {
		TiXmlElement *root = doc.RootElement();
		if (!strcmp(root->Value(), "rte")) {
			wxMessageBox(_("Invalid xml file"));
			return -1;
		}

		int i = 0;
		int myIndex = 0;
		bool nameFound = false;
		
		for (TiXmlElement* e = root->FirstChildElement(); e; e = e->NextSiblingElement(), i++) {
			
			if (!strcmp(e->Value(), "rte")) {	
				nameFound = false;
				my_points.clear();

				for (TiXmlElement* f = e->FirstChildElement(); f; f = f->NextSiblingElement()) {
					
					
					if (!strcmp(f->Value(), "name")) {
						myRte.Name = wxString::FromUTF8(f->GetText());	
						nameFound = true;						
					}
					

					if (!strcmp(f->Value(), "rtept")) {
					
						rte_lat = wxString::FromUTF8(f->Attribute("lat"));
						rte_lon = wxString::FromUTF8(f->Attribute("lon"));

						myRtePt.lat = rte_lat;
						myRtePt.lon = rte_lon;
						//wxMessageBox(_T("lat: ") + rte_lat);
						//wxMessageBox(_T("lon: ") + rte_lon);

						//wxMessageBox(wxString::FromUTF8(f->Value()));			

						for (TiXmlElement* i = f->FirstChildElement(); i; i = i->NextSiblingElement()) {

							//wxMessageBox(wxString::FromUTF8(i->Value()));

							if (!strcmp(i->Value(), "extensions")) {

								for (TiXmlElement* j = i->FirstChildElement(); j; j = j->NextSiblingElement()) {

									if (!strcmp(j->Value(), "opencpn:guid")) {
										wpt_guid = wxString::FromUTF8(j->GetText());

										myRtePt.m_GUID = wpt_guid;

										//wxMessageBox(_T("guid: ") + wpt_guid);
									}

								}
							}
						}

						myRtePt.index = myIndex;
						myIndex++;
						my_points.push_back(myRtePt);						
					}					
					
				}
				myRte.m_rteptList = my_points;
				if (!nameFound) {
					myRte.Name = _T("Unnamed");
				}
				my_routes.push_back(myRte);
				myIndex = 0;
				my_points.clear();
			}

			my_points.clear();
			myIndex = 0;
		}

		
	}


	return -1;

}

wxString Dlg::StandardPath()
{
	wxString stdPath(*GetpPrivateApplicationDataLocation());
	return stdPath;
}


void Dlg::WriteKML() {

	m_Timer->Stop();

	wxFile myFile;
	myFile.Create(m_sKmlFile, true, 755);

	double myPan = 0;
	double myCameraDir = myDir;

	if (MyCameraControl != NULL) {
		myPan = MyCameraControl->m_pan;
	}

	double fcLat, fcLon; // for camera lat/lon
	myCameraDir += myPan;
	if (myCameraDir > 360) {
		myCameraDir -= 360;
	}
	else if (myCameraDir < 0) {
		myCameraDir += 360;
	}

	double revCourse = ReverseCourse(myCameraDir);

	PositionBearingDistanceMercator_Plugin(initLat, initLon, revCourse, 0.1, &fcLat, &fcLon);

	if (myFile.IsOpened()) {

		myFile.Write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
		myFile.Write("<kml xmlns=\"http://earth.google.com/kml/2.0\">");
		myFile.Write("<Placemark id=\"aor\">");
		myFile.Write("<name>model</name>");
		myFile.Write("<LookAt>");
		myFile.Write("<longitude>");
		wxString cLon = wxString::Format("%f", fcLon);
		myFile.Write(cLon);
		myFile.Write("</longitude>");
		myFile.Write("<latitude>");
		wxString cLat = wxString::Format("%f", fcLat);
		myFile.Write(cLat);
		myFile.Write("</latitude>");
		if (MyCameraControl != NULL) {
			MyCameraControl->m_Range = MyCameraControl->m_choiceRange->GetStringSelection();
			wxString cameraRange = MyCameraControl->m_Range;
			myFile.Write("<range>");
			myFile.Write(cameraRange);
			myFile.Write("</range>");
		}
		else {
			myFile.Write("<range>5000</range>");
		}
		
		myFile.Write("<altitude>6000</altitude>");
		myFile.Write("<heading>");
		wxString sDir = wxString::Format("%f", myDir);
		myFile.Write(sDir);
		myFile.Write("</heading>");
		if (MyCameraControl != NULL) {
			wxString cameraTilt = MyCameraControl->m_textCtrlTilt->GetValue();
			myFile.Write("<tilt>");
			myFile.Write(cameraTilt);
			myFile.Write("</tilt>");
		}
		else {
			myFile.Write("<tilt>60</tilt>");
		}
		//myFile.Write("<roll>0</roll>");
		myFile.Write("<altitudeMode>clampToGround</altitudeMode>");
		myFile.Write("</LookAt>");
		// model is written here
		myFile.Write("<Model>");
		myFile.Write("<Location>");
		myFile.Write("<longitude>");
		wxString sLon = wxString::Format("%f", initLon);
		myFile.Write(sLon);
		myFile.Write("</longitude>");
		myFile.Write("<latitude>");
		wxString sLat = wxString::Format("%f", initLat);
		myFile.Write(sLat);
		myFile.Write("</latitude>");
		myFile.Write("</Location>");
		myFile.Write("<Orientation>");
		myFile.Write("<heading>");
		myFile.Write(sDir);
		myFile.Write("</heading>");
		myFile.Write("<tilt>0</tilt>");
		myFile.Write("<roll>0</roll>");
		myFile.Write("</Orientation>");
		myFile.Write("<Scale>");
		myFile.Write("<x>1</x>");
		myFile.Write("<y>1</y>");
		myFile.Write("<z>1</z>");
		myFile.Write("</Scale>");
		myFile.Write("<Link>");

		wxString modelFile = GetModelFile("container.dae");

		myFile.Write("<href>");
		myFile.Write(modelFile);		
		myFile.Write("</href>");
		myFile.Write("</Link>");
		myFile.Write("</Model>");

		myFile.Write("</Placemark>");
		myFile.Write("</kml>");
		//... write some more
		myFile.Close();
	}

	m_Timer->Start();
}

double Dlg::ReverseCourse(double initCourse) {

	double twa = initCourse + 180;
	if (twa >= 360) {
		twa -= 360;
	}
	return twa;
}

void Dlg::OnCamera(wxCommandEvent& event) {
    //Toggle 
	m_bShowCamera = !m_bShowCamera;
	//wxMessageBox("camera");
	if (MyCameraControl == NULL){

		MyCameraControl = new MyCamera(GetParent());
		MyCameraControl->m_cameraVertical = "60";
		MyCameraControl->m_cameraHorizontal = "0";
				
	}
	if (m_bShowCamera) {
		MyCameraControl->Show();
		m_bShowCamera = true;
		m_buttonCamera->SetBackgroundColour(wxColour(255, 0, 0));
	}
	else {
		MyCameraControl->Hide();
		m_bShowCamera = false;
		m_buttonCamera->SetBackgroundColour(wxColour(0, 255, 0));
	}
}

MyCamera::MyCamera(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);
	
	wxBoxSizer* bSizer12;
	bSizer12 = new wxBoxSizer(wxHORIZONTAL);

	wxBoxSizer* bSizer10;
	bSizer10 = new wxBoxSizer(wxVERTICAL);

	m_staticline3 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
	bSizer10->Add(m_staticline3, 0, wxEXPAND | wxALL, 5);

	wxGridSizer* gSizer2;
	gSizer2 = new wxGridSizer(3, 3, 0, 0);

	m_staticText11 = new wxStaticText(this, wxID_ANY, _("Range (m)"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText11->Wrap(-1);
	m_staticText11->SetFont(wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Arial")));

	gSizer2->Add(m_staticText11, 0, wxALL, 5);

	wxString m_choiceRangeChoices[] = { _("1000"), _("2000"), _("3000"), _("4000"), _("5000"), _("6000") };
	int m_choiceRangeNChoices = sizeof(m_choiceRangeChoices) / sizeof(wxString);
	m_choiceRange = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceRangeNChoices, m_choiceRangeChoices, 0);
	m_choiceRange->SetSelection(4);
	m_choiceRange->SetFont(wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Arial")));

	gSizer2->Add(m_choiceRange, 0, wxALL, 5);

	m_button14 = new wxButton(this, wxID_ANY, _("Reset All"), wxDefaultPosition, wxDefaultSize, 0);
	m_button14->SetFont(wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Arial")));
	m_button14->SetBackgroundColour(wxColour(128, 255, 128));

	gSizer2->Add(m_button14, 0, wxALL, 5);

	m_staticText81 = new wxStaticText(this, wxID_ANY, _("Pan/Tilt"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText81->Wrap(-1);
	m_staticText81->SetFont(wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Arial")));

	gSizer2->Add(m_staticText81, 0, wxALL, 5);

	m_staticText8 = new wxStaticText(this, wxID_ANY, _("Tilt    "), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText8->Wrap(-1);
	m_staticText8->SetFont(wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Arial")));

	gSizer2->Add(m_staticText8, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 5);

	m_textCtrlTilt = new wxTextCtrl(this, wxID_ANY, _("60"), wxDefaultPosition, wxDefaultSize, 0);
	m_textCtrlTilt->SetFont(wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Arial")));

	gSizer2->Add(m_textCtrlTilt, 0, wxALIGN_CENTER_VERTICAL, 5);

	m_staticText9 = new wxStaticText(this, wxID_ANY, _("Pan    port/stbd"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText9->Wrap(-1);
	m_staticText9->SetFont(wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Arial")));

	gSizer2->Add(m_staticText9, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT | wxALL, 5);

	m_textCtrlPan = new wxTextCtrl(this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, wxTE_CENTER);
	m_textCtrlPan->SetFont(wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Arial")));

	gSizer2->Add(m_textCtrlPan, 0, wxALIGN_BOTTOM | wxALL, 5);

	m_staticText10 = new wxStaticText(this, wxID_ANY, _("Starboard"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText10->Wrap(-1);
	m_staticText10->SetFont(wxFont(14, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Arial")));
	m_staticText10->Hide();

	gSizer2->Add(m_staticText10, 0, 0, 5);


	bSizer10->Add(gSizer2, 0, 0, 5);

	m_sliderPan = new wxSlider(this, wxID_ANY, 0, -90, 90, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	m_sliderPan->SetBackgroundColour(wxColour(255, 187, 255));

	bSizer10->Add(m_sliderPan, 0, wxEXPAND, 5);

	m_staticline1 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
	bSizer10->Add(m_staticline1, 0, wxEXPAND | wxALL, 5);


	bSizer12->Add(bSizer10, 0, 0, 5);

	m_sliderTilt = new wxSlider(this, wxID_ANY, 60, 0, 90, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL);
	m_sliderTilt->SetBackgroundColour(wxColour(159, 207, 255));

	bSizer12->Add(m_sliderTilt, 0, wxEXPAND, 5);

	m_staticline2 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL);
	bSizer12->Add(m_staticline2, 0, wxEXPAND | wxALL, 5);


	this->SetSizer(bSizer12);
	this->Layout();
	bSizer12->Fit(this);

	this->Centre(wxBOTH);

	// Connect Events
	m_choiceRange->Connect(wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler(MyCamera::OnChoiceRange), NULL, this);
	m_button14->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyCamera::OnReset), NULL, this);
	m_sliderPan->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Connect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Connect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Connect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Connect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Connect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Connect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderTilt->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Connect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Connect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Connect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Connect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Connect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Connect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
}

MyCamera::~MyCamera()
{
	// Disconnect Events
	m_choiceRange->Disconnect(wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler(MyCamera::OnChoiceRange), NULL, this);
	m_button14->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyCamera::OnReset), NULL, this);
	m_sliderPan->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Disconnect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Disconnect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Disconnect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Disconnect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Disconnect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Disconnect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderPan->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(MyCamera::OnScrollPan), NULL, this);
	m_sliderTilt->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Disconnect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Disconnect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Disconnect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Disconnect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Disconnect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Disconnect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);
	m_sliderTilt->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(MyCamera::OnScrollTilt), NULL, this);

}


void MyCamera::OnScrollTilt(wxScrollEvent& event) {

	double tilt = m_sliderTilt->GetValue();
	m_cameraVertical = wxString::Format("%f", tilt);
	m_textCtrlTilt->SetValue(m_cameraVertical);	

}

void MyCamera::OnScrollPan(wxScrollEvent& event) {

	m_pan = m_sliderPan->GetValue();
	m_cameraHorizontal = wxString::Format("%f", m_pan);
	m_textCtrlPan->SetValue(m_cameraHorizontal);

}

void MyCamera::OnChoiceRange(wxCommandEvent& event) {
	m_Range = m_choiceRange->GetStringSelection();
}


void MyCamera::OnReset(wxCommandEvent& event) {

	m_sliderPan->SetValue(0);
	m_textCtrlPan->SetValue("0");
	
	m_sliderTilt->SetValue(60);
	m_textCtrlTilt->SetValue("60");

	m_choiceRange->SetSelection(4);

}

