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
	SLIDER_PIXEL_CLOCK,
	SLIDER_FRAME_TIME,
	SLIDER_EXPOSURE,
	BUTTON_CONNECT_BEGIN,
	BUTTON_CONNECT_END = BUTTON_CONNECT_BEGIN + MAX_CAMERA_NUMBER
};

class MainFrame;
class CameraManager;

class MainApp: public wxApp
{
	public:
	MainApp();
	virtual bool OnInit();
	virtual int OnExit();
	
	bool openCamera(const std::string &id, uint64_t cameraId);
	bool closeCamera(const std::string &id);
	bool openCamera(const ueye::CameraInfo &camera);
	bool closeCamera(const ueye::CameraInfo &camera);
	bool isOpen(const ueye::CameraInfo &camera);
	
	void updateCurrentCamera();
	CameraManager* getCurrentCamera();
	
	private:
	
	static std::string cameraId(const ueye::CameraInfo &camera);
	
	MainFrame *Frame;
	std::map<std::string, CameraManager*> Cameras;
};
DECLARE_APP(MainApp)

class DisplayPanel;
class ConfigurationPanel;
class MainFrame: public wxFrame
{
	public:
	MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	
	DisplayPanel *Display;
	ConfigurationPanel *Configuration;
	private:
	void OnExit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	
	wxDECLARE_EVENT_TABLE();
};

class ConfigurationPanel: public wxNotebook
{
	public:
	ConfigurationPanel(wxWindow *parent);
	
	void setActiveCamera(CameraManager *cameraManager);
};

class CameraConfigurationBase: public wxPanel
{
	public:
	CameraConfigurationBase(wxWindow *parent);
	
	virtual void setActiveCamera(CameraManager *cameraManager);
	
	protected:
	CameraManager *CurrentCamera;
};

class CameraSelectionPanel: public CameraConfigurationBase
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

class CameraTimingPanel: public CameraConfigurationBase
{
	public:
	CameraTimingPanel(wxWindow *parent);
	
	virtual void setActiveCamera(CameraManager *cameraManager);
	
	private:
	void OnPixelClockSlider(wxScrollEvent &event);
	void OnFrameTimeSlider(wxScrollEvent &event);
	void OnExposureSlider(wxScrollEvent &event);
	void update();
	
	wxSlider *PixelClockSlider, *FrameTimeSlider, *ExposureSlider;
	
	wxDECLARE_EVENT_TABLE();
};

class DisplayPanel: public wxNotebook
{
	public:
	DisplayPanel(wxWindow *parent);
	
	virtual bool AddPage(wxWindow* page, const wxString& text, bool select = false, int imageId = NO_IMAGE);
	
	private:
	void OnPageChanged(wxBookCtrlEvent& event);
	wxDECLARE_EVENT_TABLE();
	double DisplayRate;
};

class DisplayTimer;
class CameraDisplay: public wxGLCanvas
{
	public:
	CameraDisplay(wxWindow *parent);
	virtual ~CameraDisplay();
	
	void setImage(ueye::ImageMemory *image);
	
	void start(int interval_ms);
	void stop();
	
	private:
	
	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	
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
	
	ueye::Camera *Camera;
	
	private:
	void liveCaptureLoop();
	
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

BEGIN_EVENT_TABLE(DisplayPanel, wxNotebook)
	EVT_NOTEBOOK_PAGE_CHANGED(wxID_ANY, DisplayPanel::OnPageChanged)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(CameraTimingPanel, wxPanel)
	EVT_COMMAND_SCROLL_CHANGED(SLIDER_PIXEL_CLOCK, CameraTimingPanel::OnPixelClockSlider)
	EVT_COMMAND_SCROLL(SLIDER_FRAME_TIME, CameraTimingPanel::OnFrameTimeSlider)
	EVT_COMMAND_SCROLL(SLIDER_EXPOSURE, CameraTimingPanel::OnExposureSlider)
END_EVENT_TABLE()

wxIMPLEMENT_APP(MainApp);

MainApp::MainApp():
	wxApp(), Frame(NULL)
{}

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
	CameraDisplay *display = new CameraDisplay(Frame->Display);
	ueye::Camera *camera = new ueye::Camera(cameraId);
	Frame->Display->AddPage(display, id, true);
	Cameras[id] = new CameraManager(camera, display);
	Cameras[id]->startLiveCapture();
	updateCurrentCamera();
	return true;
}

bool MainApp::closeCamera(const std::string &id)
{
	if(Frame)
	{
		for(size_t i=0; i<Frame->Display->GetPageCount(); ++i)
		{
			if(Frame->Display->GetPageText(i) == id)
			{
				Frame->Display->DeletePage(i);
				break;
			}
		}
		Frame->Display->Layout();
	}
	Cameras[id]->stopLiveCapture();
	delete Cameras[id];
	Cameras.erase(id);
	updateCurrentCamera();
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

CameraManager* MainApp::getCurrentCamera()
{
	if(!Frame || !Frame->Display)
		return NULL;
	int selected = Frame->Display->GetSelection();
	if(selected != wxNOT_FOUND)
	{
		return Cameras[std::string(Frame->Display->GetPageText(selected))];
	}
	return NULL;
}

void MainApp::updateCurrentCamera()
{
	if(Frame && Frame->Configuration)
	{
		CameraManager* cameraManager = getCurrentCamera();
		Frame->Configuration->setActiveCamera(cameraManager);
	}
}

MainFrame::MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame(NULL, wxID_ANY, title, pos, size), Display(NULL)
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
	Configuration = new ConfigurationPanel(this);
	// create video viewer
	Display = new DisplayPanel(this);
	// create and fill sizer with panel and vidoe viewer
	wxBoxSizer *mainSizer = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(Configuration, 1, wxEXPAND, 0);
	mainSizer->Add(Display, 3, wxEXPAND, 0);
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

ConfigurationPanel::ConfigurationPanel(wxWindow *parent):
	wxNotebook(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, "configuration panel")
{
	AddPage(new CameraSelectionPanel(this), "camera", true);
	AddPage(new CameraTimingPanel(this), "timing");
}

void ConfigurationPanel::setActiveCamera(CameraManager *cameraManager)
{
	for(size_t i=0; i<GetPageCount(); ++i)
	{
		CameraConfigurationBase *config = dynamic_cast<CameraConfigurationBase*>(GetPage(i));
		if(config)
		{
			config->setActiveCamera(cameraManager);
		}
	}
}

CameraConfigurationBase::CameraConfigurationBase(wxWindow *parent):
	wxPanel(parent), CurrentCamera(NULL)
{}

void CameraConfigurationBase::setActiveCamera(CameraManager *cameraManager)
{
	CurrentCamera = cameraManager;
}

CameraSelectionPanel::CameraSelectionPanel(wxWindow *parent):
	CameraConfigurationBase(parent), DisplayGrid(NULL), ConnectButtonSizer(NULL), GridRowHeight(25)
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

CameraTimingPanel::CameraTimingPanel(wxWindow *parent):
	CameraConfigurationBase(parent), PixelClockSlider(NULL), FrameTimeSlider(NULL), ExposureSlider(NULL)
{
	PixelClockSlider = new wxSlider(this, SLIDER_PIXEL_CLOCK, 0, 0, 0);
	FrameTimeSlider = new wxSlider(this, SLIDER_FRAME_TIME, 0, 0, 0);
	ExposureSlider = new wxSlider(this, SLIDER_EXPOSURE, 0, 0, 0);
	
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, wxID_ANY, "Pixel clock"));
	sizer->Add(PixelClockSlider, 0, wxEXPAND);
	sizer->Add(new wxStaticText(this, wxID_ANY, "Frame time"));
	sizer->Add(FrameTimeSlider, 0, wxEXPAND);
	sizer->Add(new wxStaticText(this, wxID_ANY, "Exposure time"));
	sizer->Add(ExposureSlider, 0, wxEXPAND);
	SetSizer(sizer);
	
	setActiveCamera(wxGetApp().getCurrentCamera());
}

void CameraTimingPanel::OnPixelClockSlider(wxScrollEvent &event)
{
	uint32_t pixelClock=0;
	ueye::Range<uint32_t> pixelClockRange = CurrentCamera->Camera->getPixelClockRange();
	std::vector<uint32_t> pixelClockList = CurrentCamera->Camera->getPixelClockList();
	if(pixelClockRange.step() > 0)
	{
		pixelClock = pixelClockRange.indexToValue(event.GetPosition());
	}
	else if(event.GetPosition() < pixelClockList.size())
	{
		pixelClock = pixelClockList[event.GetPosition()];
	}
	
	if(pixelClock)
	{
		CurrentCamera->Camera->setPixelClock(pixelClock);
	}
	update();
}

void CameraTimingPanel::OnFrameTimeSlider(wxScrollEvent &event)
{
	ueye::Range<double> frameTimeRange = CurrentCamera->Camera->getFrameTimeRange();
	double frameRate = 1.0/frameTimeRange.indexToValue(event.GetPosition());
	CurrentCamera->Camera->setFrameRate(frameRate);
	update();
}

void CameraTimingPanel::OnExposureSlider(wxScrollEvent &event)
{
	ueye::Range<double> exposureRange = CurrentCamera->Camera->getExposureRange();
	double exposure = exposureRange.indexToValue(event.GetPosition());
	CurrentCamera->Camera->setExposure(exposure);
	update();
}

void CameraTimingPanel::update()
{
	if(CurrentCamera)
	{
		ueye::Camera *camera = CurrentCamera->Camera;
		ueye::Range<uint32_t> pixelClockRange = camera->getPixelClockRange();
		ueye::Range<double> frameTimeRange = camera->getFrameTimeRange();
		ueye::Range<double> exposureRange = camera->getExposureRange();
		std::vector<uint32_t> pixelClockList = camera->getPixelClockList();
		if(pixelClockRange.step() > 0)
		{
			PixelClockSlider->SetRange(0, pixelClockRange.stepCount()-1);
		}
		else
		{
			PixelClockSlider->SetRange(0, pixelClockList.size()-1);
		}
		FrameTimeSlider->SetRange(0, frameTimeRange.stepCount()-1);
		ExposureSlider->SetRange(0, exposureRange.stepCount()-1);
		
		uint32_t pixelClock = camera->getPixelClock();
		double frameRate = camera->getFrameRate();
		double exposure = camera->getExposure();
		if(pixelClockRange.step() > 0)
		{
			PixelClockSlider->SetValue(pixelClockRange.valueToIndex(pixelClock));
		}
		else
		{
			auto it = std::find(pixelClockList.begin(), pixelClockList.end(), pixelClock);
			if(it!=pixelClockList.end())
				PixelClockSlider->SetValue(std::distance(pixelClockList.begin(), it));
		}
		FrameTimeSlider->SetValue(frameTimeRange.valueToIndex(1.0/frameRate));
		ExposureSlider->SetValue(exposureRange.valueToIndex(exposure));
		
		PixelClockSlider->Enable(true);
		FrameTimeSlider->Enable(true);
		ExposureSlider->Enable(true);
	}
	else
	{
		PixelClockSlider->SetRange(0,0);
		FrameTimeSlider->SetRange(0,0);
		ExposureSlider->SetRange(0,0);
		PixelClockSlider->Enable(false);
		FrameTimeSlider->Enable(false);
		ExposureSlider->Enable(false);
	}
}

void CameraTimingPanel::setActiveCamera(CameraManager *cameraManager)
{
	CurrentCamera = cameraManager;
	update();
}

DisplayPanel::DisplayPanel(wxWindow *parent):
	wxNotebook(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, "display panel"), DisplayRate(60.0)
{}

bool DisplayPanel::AddPage(wxWindow* page, const wxString& text, bool select, int imageId)
{
	CameraDisplay* display =  dynamic_cast<CameraDisplay*>(GetCurrentPage());
	if(display)
		display->stop();
		
	wxNotebook::AddPage(page, text, select, imageId);
	
	display = dynamic_cast<CameraDisplay*>(page);
	if(display)
		display->start(1000.0/DisplayRate);
}

void DisplayPanel::OnPageChanged(wxBookCtrlEvent& event)
{
	int old_page = event.GetOldSelection();
	int new_page = event.GetSelection();
	if(old_page != wxNOT_FOUND)
	{
		CameraDisplay *display = dynamic_cast<CameraDisplay*>(GetPage(old_page));
		if(display)
			display->stop();
	}
	if(new_page != wxNOT_FOUND)
	{
		CameraDisplay *display = dynamic_cast<CameraDisplay*>(GetPage(new_page));
		if(display)
			display->start(1000.0/DisplayRate);
	}
	wxGetApp().updateCurrentCamera();
}


CameraDisplay::CameraDisplay(wxWindow *parent):
	wxGLCanvas(parent, wxID_ANY, NULL), Context(NULL), Image(NULL), Timer(NULL)
{
	Context = new wxGLContext(this);
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

void CameraDisplay::start(int interval_ms)
{
	if(Timer)
		stop();
	Timer = new DisplayTimer(this, interval_ms);
}

void CameraDisplay::stop()
{
	delete Timer;
	Timer = NULL;
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
	Camera->videoCaptureStart(Buffer);
	CaptureThread = new std::thread(&CameraManager::liveCaptureLoop, this);
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
		ueye::ImageMemory *frame = Camera->waitNextFrame(4294967295);
		Display->setImage(frame);
		if(previous_frame)
			Camera->unlockFrame(previous_frame);
		previous_frame = frame;
	}
	if(previous_frame)
		Camera->unlockFrame(previous_frame);
}

