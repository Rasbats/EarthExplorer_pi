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

#ifndef _HRGUI_IMPL_H_
#define _HRGUI_IMPL_H_

#ifdef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "EarthExplorergui.h"
#include "EarthExplorer_pi.h"
#include "AisMaker.h"
#include <wx/utils.h>
#include <wx/gdicmn.h>
#include <sstream>
#include <cmath>
#include <wx/stdpaths.h>
#include "wx/process.h"
#include "jsoncpp/json/reader.h"
#include "jsoncpp/json/writer.h"
#include <wx/thread.h>
#include "tinyxml.h"
#include "tinystr.h"
#include <wx/filedlg.h>
#include "ocpn_plugin.h"
#include <wx/listctrl.h>
#include <wx/statline.h>

#ifdef __WXOSX__
#define EARTHEXPLORER_DLG_STYLE wxCLOSE_BOX|wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxSTAY_ON_TOP
#else
#define EARTHEXPLORER_DLG_STYLE wxCLOSE_BOX|wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER
#endif
using namespace std;

class EarthExplorer_pi;
class MyCamera;


// ----------------------------------------------------------------------------
// a simple thread
// ----------------------------------------------------------------------------

class rtept
{
public:

	wxString Name, m_GUID;
	int index;
	wxString lat, lon;

};

class rte
{
public:

	wxString Name;

	vector<rtept> m_rteptList;

};


class AisMaker;

class Dlg : public EarthExplorerBase
{
public:
	Dlg(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("EarthExplorer"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = EARTHEXPLORER_DLG_STYLE);
	EarthExplorer_pi *plugin;

	wxString createVHWSentence(double stw, double hdg);
	wxString createMWVTSentence(double spd, double hdg, double winddirection, double windspeed);
	wxString createMWVASentence(double spd, double hdg, double winddirection, double windspeed);
	wxString createMWDSentence(double winddirection, double windspeed);
	wxString createRMCSentence(wxDateTime myTime, double myLat, double myLon, double mySpd, double myDir);
	wxString createGLLSentence(wxDateTime myTime, double myLat, double myLon, double mySpd, double myDir);
	wxString createVTGSentence(double mySpd, double myDir);
	wxString createHDTSentence(double myDir);

	wxString LatitudeToString(double mLat);
	wxString LongitudeToString(double mLon);
	wxString DateTimeToTimeString(wxDateTime myDT);
	wxString DateTimeToDateString(wxDateTime myDT);
	void OnContextMenu(double m_lat, double m_lon);

	wxString makeCheckSum(wxString mySentence);

	wxTimer *   m_Timer;
	void OnTimer(wxTimerEvent& event);

	double GetLatitude()  { return initLat; };
	double GetLongitude() { return initLon; };

	double initLat;
	double initLon;
	double nextLat;
	double nextLon;

	double stepLat;
	double stepLon;
	AisMaker* myAIS;
	wxTextFile* nmeafile;

	bool m_bUseAis;
	bool m_bUseFile;
	wxString m_tMMSI;

	bool m_bAuto;
	wxDateTime m_GribTimelineTime;

	double myDir;
	MyCamera *MyCameraControl;
	bool m_bShowCamera;
	wxString m_sKmlFile;
	wxString SetFileKML(wxString kmlFile);
	wxString GetModelFile(wxString modelFile);

protected:
	bool m_bNeedsGrib;

private:
	void Notify();
	wxString MWD, VHW, MWVA, MWVT, GLL, VTG, HDT;
	double initDir, initSpd, initRudder, myDist, followStepDistance;

	vector<rte> my_routes;
	vector<rtept> routePoints;

	int nextRoutePointIndex;
	double nextRoutePoint;
	double followDir;
	int countRoutePoints;

	wxDateTime dt;
	void SetInterval(int interval);
	int  m_interval;

	wxString m_sTimeSentence;
	wxString m_sTimeID;
	wxString m_sNmeaTime;

	bool dbg;

	bool m_bUseSetTime;
	bool m_bUseStop;
	bool m_bUsePause;

	wxString ParseNMEAIdentifier(wxString sentence);
	wxString ParseNMEASentence(wxString sentence, wxString id);

	void SetNextStep(double inLat, double inLon, double inDir, double inSpd, double &outLat, double &outLon);
	void SetFollowStep(double inLat, double inLon, double inDir, double inSpd, double &outLat, double &outLon);

	void OnStart(wxCommandEvent& event);
	void OnStop(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);

	void SetStop();
	void StartDriving();

	int mainTest(int argc, char *argv[]);
	//void SendAIS(double cse, double spd, double lat, double lon);

	void OnMidships(wxCommandEvent& event);
	void OnMinus10(wxCommandEvent& event);
	void OnPlus10(wxCommandEvent& event);
	void OnMinus1(wxCommandEvent& event);
	void OnPlus1(wxCommandEvent& event);
	
	void OnStandby(wxCommandEvent& event);
	void GoToStandby();

	void OnAuto(wxCommandEvent& event);

	
	long m_iMMSI;

	virtual void Lock() { routemutex.Lock(); }
	virtual void Unlock() { routemutex.Unlock(); }
	wxMutex routemutex;


	void RequestGrib(wxDateTime time);
	bool GetGribSpdDir(wxDateTime dt, double lat, double lon, double &spd, double &dir);
	void OnWind(wxCommandEvent& event);
	double GetPolarSpeed(double lat, double lon, double cse);

	double AttributeDouble(TiXmlElement *e, const char *name, double def);
	double ReadPolars(wxString filename, double windangle, double windspeed);

	double ReadNavobj();
	static wxString StandardPath();
	void WriteKML();
	double ReverseCourse(double initCourse);
	void OnCamera(wxCommandEvent& event);

	bool m_bUsingWind;
	bool m_bUsingFollow;
	bool m_bInvalidPolarsFile;
	bool m_bInvalidGribFile;
	bool m_bEarthExplorerHasStarted;
	

};

class MyCamera : public wxDialog
{
private:

	

protected:

	wxStaticLine* m_staticline3;
	wxStaticText* m_staticText11;
	
	wxButton* m_button14;
	wxStaticText* m_staticText81;
	wxStaticText* m_staticText8;
	
	wxStaticText* m_staticText9;
	
	wxStaticText* m_staticText10;
	wxSlider* m_sliderPan;
	wxStaticLine* m_staticline1;
	wxSlider* m_sliderTilt;
	wxStaticLine* m_staticline2;

	// Virtual event handlers, overide them in your derived class
	void OnChoiceRange(wxCommandEvent& event);
	void OnReset(wxCommandEvent& event);
	void OnScrollPan(wxScrollEvent& event);
	void OnScrollTilt(wxScrollEvent& event);


public:

	MyCamera* thisCamera;

	wxTextCtrl* m_textCtrlTilt;
	wxTextCtrl* m_textCtrlPan;
	wxChoice* m_choiceRange;

	wxString m_cameraVertical;
	wxString m_cameraHorizontal;
	wxString m_Range;

	double m_pan;

	MyCamera(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = "Camera Controller", const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxCAPTION);
	~MyCamera();

};

#endif



