#include <wx/wxprec.h>
#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

#include "ueye.hpp"

#include <wx/notebook.h>
#include <wx/grid.h>
#include <wx/glcanvas.h>

#include <thread>
#include <mutex>
#include <atomic>

#define MAX_CAMERA_NUMBER 256
enum
{
	BUTTON_UPDATE_CAMERA_LIST = wxID_HIGHEST + 1,
	BUTTON_CONNECT_BEGIN,
	BUTTON_CONNECT_END = BUTTON_CONNECT_BEGIN + MAX_CAMERA_NUMBER
};

class MainFrame;
class CameraManager;

class MainApp: public wxApp
{
	public:
	virtual bool OnInit();
	virtual int OnExit();
	
	bool openCamera(const std::string &id, uint64_t cameraId);
	bool closeCamera(const std::string &id);
	bool openCamera(const ueye::CameraInfo &camera);
	bool closeCamera(const ueye::CameraInfo &camera);
	bool isOpen(const ueye::CameraInfo &camera);
	
	private:
	
	static std::string cameraId(const ueye::CameraInfo &camera);
	
	MainFrame *Frame;
	std::map<std::string, CameraManager*> Cameras;
};
DECLARE_APP(MainApp)


class MainFrame: public wxFrame
{
	public:
	MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	
	wxNotebook *DisplayPanel;
	private:
	void OnExit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	
	wxDECLARE_EVENT_TABLE();
};

class CameraSelectionPanel: public wxPanel
{
	public:
	CameraSelectionPanel(wxWindow *parent);
	
	void OnOpen(wxCommandEvent& event);
	void OnClose(wxCommandEvent& event);
	
	private:
	void updateList();
	void OnUpdateButton(wxCommandEvent& event);
	
	wxGrid *DisplayGrid;
	wxBoxSizer *ConnectButtonSizer;
	int GridRowHeight;
	std::vector<ueye::CameraInfo> CameraList;
	wxDECLARE_EVENT_TABLE();
};

class ConnectionButton: public wxButton
{
	public:
	ConnectionButton(wxWindow *parent, size_t index, const wxSize &size, const ueye::CameraInfo *camera);
	
	private:
	void updateState();
	
	const ueye::CameraInfo *Camera;
	bool Opened;
};

class DisplayTimer;
class CameraDisplay: public wxGLCanvas
{
	public:
	CameraDisplay(wxWindow *parent);
	virtual ~CameraDisplay();
	
	void setImage(ueye::ImageMemory *image);
	
	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	
	private:
	void init();
	void render();
	
	wxGLContext* Context;
	ueye::ImageMemory *Image;
	std::mutex ImageMutex;
	DisplayTimer *Timer;
	
	DECLARE_EVENT_TABLE()
};

class DisplayTimer: public wxTimer
{
	public:
	DisplayTimer(CameraDisplay * display, int interval_ms);
	virtual void Notify();
	
	private:
	CameraDisplay *Display;
	int Interval;
};

class CameraManager
{
	public:
	CameraManager(ueye::Camera *camera, CameraDisplay *display);
	~CameraManager();
	
	void startLiveCapture();
	void stopLiveCapture();
	private:
	void liveCaptureLoop();
	
	ueye::Camera *Camera;
	CameraDisplay *Display;
	std::vector<ueye::ImageMemory> Buffer;
	std::thread *CaptureThread;
	std::atomic<bool> CaptureStop;
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
	EVT_MENU(wxID_EXIT,  MainFrame::OnExit)
	EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(CameraSelectionPanel, wxPanel)
	EVT_BUTTON(BUTTON_UPDATE_CAMERA_LIST, CameraSelectionPanel::OnUpdateButton)
wxEND_EVENT_TABLE()

BEGIN_EVENT_TABLE(CameraDisplay, wxGLCanvas)
	EVT_PAINT(CameraDisplay::OnPaint)
	EVT_SIZE(CameraDisplay::OnSize)
END_EVENT_TABLE()

wxIMPLEMENT_APP(MainApp);

bool MainApp::OnInit()
{
	Frame = new MainFrame( "UEye GUI", wxDefaultPosition, wxDefaultSize);
	Frame->Maximize(true);
	Frame->Show(true);
	return true;
}

int MainApp::OnExit()
{
	Frame = NULL;
	for(auto it=Cameras.begin(); it!=Cameras.end(); ++it)
	{
		closeCamera(it->first);
	}
}

bool MainApp::openCamera(const std::string &id, uint64_t cameraId)
{
	CameraDisplay *display = new CameraDisplay(Frame->DisplayPanel);
	ueye::Camera *camera = new ueye::Camera(cameraId);
	Frame->DisplayPanel->AddPage(display, id, true);
	Cameras[id] = new CameraManager(camera, display);
	Cameras[id]->startLiveCapture();
	return true;
}

bool MainApp::closeCamera(const std::string &id)
{
	if(Frame)
	{
		for(size_t i=0; i<Frame->DisplayPanel->GetPageCount(); ++i)
		{
			if(Frame->DisplayPanel->GetPageText(i) == id)
			{
				Frame->DisplayPanel->DeletePage(i);
				break;
			}
		}
		Frame->DisplayPanel->Layout();
	}
	Cameras[id]->stopLiveCapture();
	delete Cameras[id];
	Cameras.erase(id);
	return true;
}

bool MainApp::openCamera(const ueye::CameraInfo &camera)
{
	return openCamera(cameraId(camera), camera.CameraId);
}

bool MainApp::closeCamera(const ueye::CameraInfo &camera)
{
	return closeCamera(cameraId(camera));
}

bool MainApp::isOpen(const ueye::CameraInfo &camera)
{
	std::string id = camera.SerialNumber;
	return Cameras.count(id);
}

std::string MainApp::cameraId(const ueye::CameraInfo &camera)
{
	return camera.SerialNumber;
}

MainFrame::MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame(NULL, wxID_ANY, title, pos, size), DisplayPanel(NULL)
{
	// create menu
	wxMenu *menuFile = new wxMenu;
	menuFile->Append(wxID_EXIT);
	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(wxID_ABOUT);
	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, "&File");
	menuBar->Append(menuHelp, "&Help");
	SetMenuBar(menuBar);
	// create status bar
	CreateStatusBar();
	SetStatusText("Status");
	// create window content (configuration panel and video viewer, side by side)
	// create panel
	wxNotebook *configurationPanel = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, "configuration panel");
	configurationPanel->AddPage(new CameraSelectionPanel(configurationPanel), "camera", true);
	// create video viewer
	DisplayPanel = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, "display panel");
	// create and fill sizer with panel and vidoe viewer
	wxBoxSizer *mainSizer = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(configurationPanel, 1, wxEXPAND, 0);
	mainSizer->Add(DisplayPanel, 3, wxEXPAND, 0);
	SetSizer(mainSizer);
}

void MainFrame::OnExit(wxCommandEvent& event)
{
	Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& event)
{
	wxMessageBox("IDS Ueye camera test interface",
		"About UEye GUI", wxOK | wxICON_INFORMATION );
}

CameraSelectionPanel::CameraSelectionPanel(wxWindow *parent):
	wxPanel(parent), DisplayGrid(NULL), ConnectButtonSizer(NULL), GridRowHeight(25)
{
	DisplayGrid = new wxGrid(this, wxID_ANY);
	wxButton *updateButton = new wxButton(this, BUTTON_UPDATE_CAMERA_LIST, "Update");
	DisplayGrid->CreateGrid(0, 5);
	DisplayGrid->SetRowLabelSize(0);
	DisplayGrid->SetColLabelSize(GridRowHeight);
	DisplayGrid->SetColLabelValue(0, "Name");
	DisplayGrid->AutoSizeColLabelSize(0);
	DisplayGrid->SetColLabelValue(1, "Cam ID");
	DisplayGrid->AutoSizeColLabelSize(1);
	DisplayGrid->SetColLabelValue(2, "In use");
	DisplayGrid->AutoSizeColLabelSize(2);
	DisplayGrid->SetColLabelValue(3, "Status");
	DisplayGrid->AutoSizeColLabelSize(3);
	DisplayGrid->SetColLabelValue(4, "Serial");
	DisplayGrid->AutoSizeColLabelSize(4);
	
	ConnectButtonSizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *gridSizer = new wxBoxSizer(wxHORIZONTAL);
	gridSizer->Add(DisplayGrid, 0, wxEXPAND);
	gridSizer->Add(ConnectButtonSizer, 0, wxEXPAND);
	
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(gridSizer, 0, wxEXPAND);
	sizer->Add(updateButton, 0, wxEXPAND);
	SetSizer(sizer);
	
	updateList();
}

void CameraSelectionPanel::updateList()
{
	CameraList = ueye::getCameraList();
	int rows = DisplayGrid->GetNumberRows();
	if(rows)
		DisplayGrid->DeleteRows(0, rows);
	DisplayGrid->InsertRows(0, CameraList.size());
	
	while(ConnectButtonSizer->GetItemCount())
	{
		wxWindow *win = ConnectButtonSizer->GetItem((size_t)0)->GetWindow();
		ConnectButtonSizer->Detach(0);
		delete win;
	}
	
	ConnectButtonSizer->AddSpacer(GridRowHeight);
	for(size_t i=0; i<CameraList.size(); ++i)
	{
		DisplayGrid->SetRowSize(i, GridRowHeight);
		DisplayGrid->SetCellValue(i, 0, CameraList[i].FullModelName);
		DisplayGrid->SetReadOnly(i, 0);
		DisplayGrid->SetCellValue(i, 1, std::to_string(CameraList[i].CameraId));
		DisplayGrid->SetReadOnly(i, 1);
		DisplayGrid->SetCellValue(i, 2, std::to_string(CameraList[i].InUse));
		DisplayGrid->SetReadOnly(i, 2);
		DisplayGrid->SetCellValue(i, 3, std::to_string(CameraList[i].CameraStatus));
		DisplayGrid->SetReadOnly(i, 3);
		DisplayGrid->SetCellValue(i, 4, CameraList[i].SerialNumber);
		DisplayGrid->SetReadOnly(i, 4);
		ConnectButtonSizer->Add(new ConnectionButton(this, i, wxSize(-1, GridRowHeight), &(CameraList[i])), 0, wxEXPAND);
	}
	GetSizer()->Layout();
}

void CameraSelectionPanel::OnUpdateButton(wxCommandEvent& event)
{
	updateList();
}

void CameraSelectionPanel::OnOpen(wxCommandEvent& event)
{
	wxGetApp().openCamera(CameraList[event.GetId()-BUTTON_CONNECT_BEGIN]);
	updateList();
}

void CameraSelectionPanel::OnClose(wxCommandEvent& event)
{
	wxGetApp().closeCamera(CameraList[event.GetId()-BUTTON_CONNECT_BEGIN]);
	updateList();
}

ConnectionButton::ConnectionButton(wxWindow *parent, size_t index, const wxSize &size, const ueye::CameraInfo *camera):
	wxButton(parent, BUTTON_CONNECT_BEGIN+index, "Open", wxDefaultPosition, size), Camera(camera), Opened(false)
{
	Opened = wxGetApp().isOpen(*Camera);
	updateState();
}

void ConnectionButton::updateState()
{
	CameraSelectionPanel *panel = dynamic_cast<CameraSelectionPanel*>(GetParent());
	Unbind(wxEVT_BUTTON, &CameraSelectionPanel::OnOpen, panel, GetId());
	Unbind(wxEVT_BUTTON, &CameraSelectionPanel::OnClose, panel, GetId());
	if(Opened)
	{
		Bind(wxEVT_BUTTON, &CameraSelectionPanel::OnClose, panel, GetId());
		SetLabel("Close");
		Enable(true);
	}
	else if(Camera->InUse)
	{
		SetLabel("Not available");
		Enable(false);
	}
	else
	{
		Bind(wxEVT_BUTTON, &CameraSelectionPanel::OnOpen, panel, GetId());
		SetLabel("Open");
		Enable(true);
	}
}


CameraDisplay::CameraDisplay(wxWindow *parent):
	wxGLCanvas(parent, wxID_ANY, NULL), Context(NULL), Image(NULL), Timer(NULL)
{
	Context = new wxGLContext(this);
	Timer = new DisplayTimer(this, 10);
	init();
}

CameraDisplay::~CameraDisplay()
{
	delete Timer;
	delete Context;
}

void CameraDisplay::setImage(ueye::ImageMemory *image)
{
	ImageMutex.lock();
	Image = image;
	ImageMutex.unlock();
}

void CameraDisplay::OnPaint(wxPaintEvent &)
{
	render();
}

void CameraDisplay::OnSize(wxSizeEvent &event)
{
	init();
	render();
}

void CameraDisplay::init()
{
	SetCurrent(*Context);
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glViewport(0, 0, GetSize().x, GetSize().y);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1, 1, -1, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void CameraDisplay::render()
{
	if(!Image)
		return;
	SetCurrent(*Context);
	wxPaintDC(this);
	glClear(GL_COLOR_BUFFER_BIT);
	ImageMutex.lock();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, Image->width(), Image->height(), 0, GL_BGR, GL_UNSIGNED_BYTE, Image->ptr());
	ImageMutex.unlock();
	glBegin(GL_QUADS);
		glTexCoord2f(0, 1); glVertex3f(-1, -1, 0);
		glTexCoord2f(0, 0); glVertex3f(-1, 1, 0);
		glTexCoord2f(1, 0); glVertex3f(1, 1, 0);
		glTexCoord2f(1, 1); glVertex3f(1, -1, 0);
	glEnd();
	glFlush();
	SwapBuffers();
}

DisplayTimer::DisplayTimer(CameraDisplay * display, int interval_ms):
	wxTimer(), Display(display), Interval(interval_ms)
{
	Start(Interval);
}

void DisplayTimer::Notify()
{
	Display->Refresh(false);
}

CameraManager::CameraManager(ueye::Camera *camera, CameraDisplay *display):
	Camera(camera), Display(display), CaptureThread(NULL)
{}

CameraManager::~CameraManager()
{
	if(CaptureThread)
	{
		stopLiveCapture();
	}
	delete Camera;
}

void CameraManager::startLiveCapture()
{
	Buffer = std::vector<ueye::ImageMemory>(3, ueye::ImageMemory(*Camera));
	CaptureStop.store(false);
	CaptureThread = new std::thread(&CameraManager::liveCaptureLoop, this);
	Camera->videoCaptureStart(Buffer);
}

void CameraManager::stopLiveCapture()
{
	CaptureStop.store(true);
	CaptureThread->join();
	Camera->videoCaptureStop();
	Buffer.clear();
	CaptureThread = NULL;
}

void CameraManager::liveCaptureLoop()
{
	ueye::ImageMemory *previous_frame = NULL;
	while(!CaptureStop.load())
	{
		ueye::ImageMemory *frame = Camera->waitNextFrame();
		Display->setImage(frame);
		if(previous_frame)
			Camera->unlockFrame(previous_frame);
		previous_frame = frame;
	}
	if(previous_frame)
		Camera->unlockFrame(previous_frame);
}

