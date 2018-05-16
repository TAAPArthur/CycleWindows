#ifndef PK_H
#define PK_H


#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <xdo.h>
#include <X11/keysym.h>

#define LEN(X) (sizeof X / sizeof X[0])
#define NUMBER_OF_MASTER_DEVICES 32
#define NUMBER_OF_WINDOWS 8
#define NONFOCUSED_WINDOW_COLOR 0xdddddd

Display *dpy = NULL;
xdo_t *xdo;
Window root;
int CYCLE_WINDOWS_END_KEY=XK_Alt_L;
Window queryWindow;

int numberOfActiveMasters=0;

int CYCLE_WINDOWS_END_KEYCODE;




typedef struct {
	int mod;
	KeySym keySym;

	KeyCode keyCode;
} Key;

typedef struct {
	Window windowOrder[NUMBER_OF_WINDOWS];
	Bool cycling;
	int offset;
	int numberOfWindows;
	Bool dirty;
} MasterWindows;

typedef struct {
	int id;
	MasterWindows windows;
} Master;




void reset();
void clear();
void setup();
void addMaster(int keyboardMasterId);
void removeMaster(int keyboardMasterId);
int getMasterIndex(int keyboardMasterId);
void initCurrentMasters();
void listenForHiearchyChange();
void listenForKeys();
int getMasterPointerId(XIDeviceEvent *devev,Bool mouseEvent);
int keypress(Master *master, int keyCode,int mods,Bool press);
void grabKey(int deviceID,int keyCode,int mod,Bool grab);
void detectEvent();
void cleanup();
int saveerror(Display *dpy, XErrorEvent *ee);
void msleep(long ms);
void update();
Window getNextWindowToFocus();
void cycleWindows(Master *master,int offset);
void endCycleWindows(Master *master);
void removeWindow();
Bool addWindow(Master *master,Window id);
void dump();
void updateFocus();
Key startCycle={Mod1Mask,XK_Tab,-1};
Key startReverseCycle={Mod1Mask | ShiftMask,XK_Tab,-1};

Master masters[NUMBER_OF_MASTER_DEVICES];

unsigned long masterColors[]={
		0x00FF00,0x0000FF,0xFF0000,0x00FFFF,0xFFFFFF,0x000000
};


#endif
