#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <time.h>

#include "CycleWindows.h"

#include <assert.h>



static int handleError(Display *dpy, XErrorEvent *event){
	if(event->error_code==BadWindow){
		removeWindow(queryWindow);
		queryWindow=0;
	}
	//else{
		char buff[100];
		XGetErrorText(dpy,event->error_code,buff,40);

		printf("Ignoring Xlib error: error code %d request code %d %s\n",
				   event->error_code,
				   event->request_code,buff) ;
	//}
	return 0;
}
void removeWindow(Window winToRemove){
	printf("removing window %ld",winToRemove);
	for(int i=0;i<numberOfActiveMasters;i++){
		int index=0;
		for(;index<masters[i].windows.numberOfWindows;index++)
			if(masters[i].windows.windowOrder[i]==winToRemove)
				break;
		if(index==masters[i].windows.numberOfWindows)
			continue;
		masters[i].windows.numberOfWindows--;
		for(;index<numberOfActiveMasters;index++)
			masters[i].windows.windowOrder[i]=masters[i].windows.windowOrder[i+1];
		masters[i].windows.windowOrder[index]=0;
	}

}

void msleep(long ms){
	struct timespec duration = {
		.tv_sec=ms/1000,
		.tv_nsec=((ms % 1000) * 1e6),
	};
	nanosleep(&duration, NULL);
}
void checkXServerVersion(){
	int opcode, event, error;
	if (!XQueryExtension(dpy, "XInputExtension", &opcode, &event, &error)) {
	   printf("X Input extension not available.\n");
	   exit(1);
	}

	/* Which version of XI2? We support 2.0 */
	int major = 2, minor = 0;
	if (XIQueryVersion(dpy, &major, &minor) == BadRequest) {
	  printf("XI2 not available. Server supports %d.%d\n", major, minor);
	  exit(1);
	}
}
void init(){
	dpy = XOpenDisplay(NULL);
	xdo =xdo_new(NULL);
	checkXServerVersion();

	if (!dpy)
		exit(2);
	startCycle.keyCode=XKeysymToKeycode(dpy, startCycle.keySym);
	startReverseCycle.keyCode=XKeysymToKeycode(dpy, startReverseCycle.keySym);
	CYCLE_WINDOWS_END_KEYCODE=XKeysymToKeycode(dpy, CYCLE_WINDOWS_END_KEY);
	root = DefaultRootWindow(dpy);

	//XSelectInput(dpy, root, KeyPressMask|KeyReleaseMask);
	XSetErrorHandler(handleError);
	grabKey(XIAllMasterDevices,startCycle.keyCode,startCycle.mod,True);
	grabKey(XIAllMasterDevices,startCycle.keyCode,startCycle.mod|Mod2Mask,True);
	grabKey(XIAllMasterDevices,startReverseCycle.keyCode,startReverseCycle.mod,True);
	grabKey(XIAllMasterDevices,startReverseCycle.keyCode,startReverseCycle.mod|Mod2Mask,True);

	listenForHiearchyChange();
	initCurrentMasters();
	listenForKeys();
}
void listenForKeys(){
		XIEventMask eventmask;
		unsigned char mask[1] = { 0 }; /* the actual mask */
		eventmask.deviceid = XIAllDevices;
		eventmask.mask_len = sizeof(mask); /* always in bytes */
		eventmask.mask = mask;
		/* now set the mask */
		XISetMask(mask, XI_KeyRelease);
		XISelectEvents(dpy, root, &eventmask, 1);
}
void listenForHiearchyChange(){
	XIEventMask evmask;
	unsigned char mask[2] = { 0, 0 };

	XISetMask(mask, XI_HierarchyChanged);
	evmask.deviceid = XIAllDevices;
	evmask.mask_len = sizeof(mask);
	evmask.mask = mask;

	XISelectEvents(dpy, DefaultRootWindow(dpy), &evmask, 1);
}
void onHiearchyChange(XIHierarchyEvent *event){
	if((event->flags & (XIMasterAdded |XIMasterRemoved)) ==0)
		return;
	for (int i = 0; i < event->num_info; i++)
	    if (event->info[i].flags & XIMasterRemoved)
	    	removeMaster(event->info[i].deviceid);
	    else if (event->info[i].flags & XIMasterAdded)
	    	addMaster(event->info[i].deviceid);
}
void initCurrentMasters(){
	int ndevices;
	XIDeviceInfo *devices, *device;
	devices = XIQueryDevice(dpy, XIAllMasterDevices, &ndevices);
	for (int i = 0; i < ndevices; i++) {
		device = &devices[i];
	    if(device->use == XIMasterKeyboard )
	    	addMaster(device->deviceid);
	}

	XIFreeDeviceInfo(devices);

}

void grabKey(int deviceID,int keyCode,int mod,Bool grab){
	XIEventMask eventmask;
	unsigned char mask[1] = { 0 }; /* the actual mask */

	eventmask.deviceid = deviceID;
	eventmask.mask_len = sizeof(mask); /* always in bytes */
	eventmask.mask = mask;
	/* now set the mask */
	XISetMask(mask, XI_KeyPress);
	XISetMask(mask, XI_KeyRelease);
	KeyCode code = keyCode;
	XIGrabModifiers modifiers;
	modifiers.modifiers=mod;
	if(grab)
		XIGrabKeycode(dpy, deviceID, code, root, XIGrabModeAsync, XIGrabModeAsync, True, &eventmask, 1, &modifiers);
	else
		XIUngrabKeycode(dpy, deviceID, code, root, 1, &modifiers);
}

void test_adding(){
	reset();
	int masterID=1;
	addMaster(masterID);
	int index=getMasterIndex(masterID);
	assert(index==0);
	assert(masters[index].id==masterID);

	Window windows[NUMBER_OF_WINDOWS];
	for(int i=0;i<NUMBER_OF_WINDOWS;i++)
		windows[i]=(i+1)*100;
	for(int i=0;i<NUMBER_OF_WINDOWS;i++){
		addWindow(&masters[index], windows[i]);
		assert(masters[index].windows.numberOfWindows==i+1);
		for(int n=0;n<=i;n++)
			assert(masters[index].windows.windowOrder[n]==windows[i-n]);
	}

}
void test_removing(){
	reset();
	int masterID=1;
	addMaster(masterID);
	int index=getMasterIndex(masterID);
	Window wins[]={1,2,3,4};
	int numWin=4;
	int mul=10000;
	for(int i=0;i<numWin;i++){
		addWindow(&masters[index], wins[i]*mul);
		assert(masters[index].windows.numberOfWindows==i);
	}
	for(int i=0;i<numWin;i++){
		removeWindow(wins[numWin-i]*mul);
		assert(masters[index].windows.numberOfWindows==numWin-i-1);
	}
}
void test_cycling(){
	reset();
	int masterID=1;
	addMaster(masterID);
	int index=getMasterIndex(masterID);
	Window win1=100,win2=200,win3=300,win4=400;
	addWindow(&masters[index], win1);
	addWindow(&masters[index], win1);
	assert(masters[index].windows.windowOrder[1]==0);
	assert(masters[index].windows.numberOfWindows==1);
	addWindow(&masters[index], win2);
	addWindow(&masters[index], win1);
	addWindow(&masters[index], win2);
	addWindow(&masters[index], win3);
	addWindow(&masters[index], win4);
	addWindow(&masters[index], win3);
	addWindow(&masters[index], win2);
	addWindow(&masters[index], win4);
	addWindow(&masters[index], win2);
	addWindow(&masters[index], win1);
	assert(masters[index].windows.windowOrder[3]==win3);
	assert(masters[index].windows.windowOrder[2]==win4);
	assert(masters[index].windows.windowOrder[1]==win2);
	assert(masters[index].windows.windowOrder[0]==win1);
	assert(masters[index].windows.numberOfWindows==4);

	assert(getNextWindowToFocus(&masters[index],1)==win2);
	assert(getNextWindowToFocus(&masters[index],1)==win2);
	assert(getNextWindowToFocus(&masters[index],1)==win2);
	assert(getNextWindowToFocus(&masters[index],2)==win4);
	assert(getNextWindowToFocus(&masters[index],3)==win3);
	assert(getNextWindowToFocus(&masters[index],4)==win1);
	assert(getNextWindowToFocus(&masters[index],0)==win1);

	masters[index].windows.offset+=1;
	assert(getNextWindowToFocus(&masters[index],1)==win4);
	masters[index].windows.offset+=1;
	assert(getNextWindowToFocus(&masters[index],1)==win3);


}
void reset(){
	for(int i=0;i<numberOfActiveMasters;i++){
		masters[i].windows.cycling=False;
		masters[i].windows.numberOfWindows=0;
		masters[i].windows.offset=0;
	}
	numberOfActiveMasters=0;
}
void test(){
	test_adding();
	test_cycling();
}
int main(){
	test();
	reset();
	//return 0;
	init();
	printf("starting");
	int count=0;
	update();
	while (True){
		//workingMaster=None;
		if(XPending(dpy)){
			detectEvent();
			XFlush(dpy);

		}
		if(++count%25==0){
			update();
			count=0;
		}
		msleep(10);
		fflush(stdout);
	}
	printf("exit");

}

int getMasterIndex(int keyboardMasterId){
	for(int i=0;i<numberOfActiveMasters;i++)
			if (masters[i].id==keyboardMasterId)
				return i;
	return -1;
}
void removeMaster(int keyboardMasterId){
	int index=getMasterIndex(keyboardMasterId);
	if(index==-1)
		return;
	for(;index<numberOfActiveMasters;index++){
		masters[index-1]=masters[index];
		masters[index-1].index=index-1;
	}
	numberOfActiveMasters--;
}
void addMaster(int keyboardMasterId){
	if(getMasterIndex(keyboardMasterId)>-1)
		return;
	masters[numberOfActiveMasters].id=keyboardMasterId;
	masters[numberOfActiveMasters].index=numberOfActiveMasters;
	for(int i=0;i<NUMBER_OF_WINDOWS;i++)
		masters[numberOfActiveMasters].windows.windowOrder[i]=0;
	numberOfActiveMasters++;
}
int getAssociatedMasterDevice(int deviceId){
	int ndevices;
	XIDeviceInfo *masterDevices;
	int id;
	masterDevices = XIQueryDevice(dpy, deviceId, &ndevices);
	id=masterDevices[0].attachment;
	XIFreeDeviceInfo(masterDevices);
	return id;
}
void detectEvent(){
	XEvent event;
	//XKeyEvent ev;
	XIDeviceEvent *devev;
	while(XPending(dpy))
		XNextEvent(dpy,&event);
	XGenericEventCookie *cookie = &event.xcookie;
	//printf("EVENT TYPE %d\n", cookie->evtype);
	if(XGetEventData(dpy, cookie)){
		if (event.xcookie.type== XI_HierarchyChanged){
			onHiearchyChange(event.xcookie.data);
		}
		else if(event.xcookie.type == XI_Enter){
			printf("enter");
		}
		else if(event.xcookie.type == XI_FocusIn){
			printf("focus in");
		}
		else{
			devev = cookie->data;

			int index=getMasterIndex(devev->deviceid);
			if(index==-1)
				index=getMasterIndex(getAssociatedMasterDevice(devev->deviceid));

			//printf("%d %d\n",devev->deviceid,index);
			keypress(&masters[index],devev->detail,devev->mods.effective&~Mod2Mask,cookie->evtype==XI_KeyPress);
		}
	}
	XFreeEventData(dpy, cookie);
}

int keypress(Master *master,int keyCode,int mods,Bool press){

	//printf("key detected %d %d %d %d\n",master->id, keyCode,mods,press?1:0);
	if(press){
		if(startCycle.keyCode==keyCode && startCycle.mod==mods)
			cycleWindows(master,1);
		else if(startReverseCycle.keyCode==keyCode && startReverseCycle.mod==mods)
			cycleWindows(master,-1);
	}
	else if(keyCode==CYCLE_WINDOWS_END_KEYCODE)
		endCycleWindows(master);
	return -1;

}
/**
 * void updateFocus(){
	for(int i=0;i<numberOfActiveMasters;i++)
		if(masters[i].windows.dirty&&masters[i].windows.cycling){
			Window tempWindow=masters[i].windows.windowOrder[masters[i].windows.offset];
			XISetFocus(dpy, masters[i].id, tempWindow, 0);
			XFlush(dpy);
			sync();
			masters[i].dirty
			Window focusedWindow;
			XIGetFocus(dpy, masters[i].id, &focusedWindow);
			printf("%ld=? %ld",focusedWindow,tempWindow);
			assert(focusedWindow==tempWindow);
			dump(masters[i]);
			printf("cycle compleded\n");
		}
}
 */
void update(){
	for(int i=0;i<numberOfActiveMasters;i++)
		if(!masters[i].windows.cycling){
			Window focusedWindow;
			XIGetFocus(dpy, masters[i].id, &focusedWindow);

			//non defaults masters will return small number when no focus is set

			if(focusedWindow>10000 && addWindow(&masters[i],focusedWindow)){
				printf("new window %ld",focusedWindow);
				if(masters[i].windows.windowOrder[1]!=0)
    				resetBorder(masters[i].windows.windowOrder[1]);
				setBorder(i,focusedWindow);

			}
		}
}
void setBorder(int index,Window wid){
	XSetWindowBorder(dpy,wid,masterColors[index%LEN(masterColors)]);
}
void resetBorder(Window wid){
	if(wid==0)
		return;
	queryWindow=wid;
	for(int i=0;i<numberOfActiveMasters;i++)
		if(masters[i].windows.windowOrder[masters[i].windows.offset]==queryWindow){
			XSetWindowBorder(dpy,queryWindow,masterColors[i%LEN(masterColors)]);
			return;
		}
	XSetWindowBorder(dpy,queryWindow,NONFOCUSED_WINDOW_COLOR);
}

Window getNextWindowToFocus(Master *master, int delta){
	if(master->windows.numberOfWindows==0){
		assert(False);
		return 0;
	}
	int nextWindowIndex=(master->windows.offset+delta)%master->windows.numberOfWindows;
	Window nextWindow=master->windows.windowOrder[nextWindowIndex];

	return nextWindow;
}
int getClientKeyboard(){
	int masterPointer;
	XIGetClientPointer(dpy, 0, &masterPointer);
	return getAssociatedMasterDevice(masterPointer);
}
void cycleWindows(Master *master, int delta){
	master->windows.cycling=True;
	if(master->windows.numberOfWindows<2){
		printf("not enough windows");
		return;
	}
	Window currentWindow=master->windows.windowOrder[master->windows.offset];
	assert(currentWindow!=0);
	while(1){
		Window nextWindow=getNextWindowToFocus(master,delta);

		assert(nextWindow!=0);

		master->windows.offset=(master->windows.offset+delta)%master->windows.numberOfWindows;

		queryWindow=nextWindow;

		int clientKeyboard=getClientKeyboard();
		Window defaultMasterFocus=0;
		if(clientKeyboard!=master->id)
			XIGetFocus(dpy, clientKeyboard, &defaultMasterFocus);

		//Check if window still exists
		//XGetWindowAttributes will segfault if window is not present, so we manually
		//call it and check
		XWindowAttributes wattr;
		XGetWindowAttributes(xdo->xdpy, nextWindow, &wattr);
		XSync(dpy,False);
		if(queryWindow==0){//if window doesn't exist try again for the next window
			printf("window did not exist; trying again");
			delta=delta/abs(delta);
			continue;
		}
		printf("current window %ld; new window %ld",currentWindow,nextWindow);
		resetBorder(currentWindow);
		assert(0==xdo_activate_window(xdo,nextWindow));


		//if(newDesktop!=currentDesktop)

		XSync(dpy,False);
		if(clientKeyboard!=master->id){
			printf("adjusing default client\n");
			XISetFocus(dpy, clientKeyboard, defaultMasterFocus, 0);
		}

		XSync(dpy,False);
		printf("setting focus to %ld\n",nextWindow);
		XISetFocus(dpy, master->id, nextWindow, 0);
		setBorder(master->index, nextWindow);
		XFlush(dpy);

		Window focusedWindow;
		XIGetFocus(dpy, master->id, &focusedWindow);
		//assert(focusedWindow==nextWindow);


		//XIGrabDevice(dpy, master->id, root, 0, None, GrabModeSync, GrabModeSync, True, &eventmask);
		//printf("grabbing key %d",master->id);
		//grabKey(master->id, CYCLE_WINDOWS_END_KEYCODE, XIAnyModifier,True);
		break;
	}

}
void endCycleWindows(Master *master){
	if(!master->windows.cycling)
		return;
	XIUngrabDevice(dpy, master->id, 0);
	printf("release %d %ld; resetting \n",master->id,master->windows.windowOrder[master->windows.offset]);

	addWindow(master,master->windows.windowOrder[master->windows.offset]);
	master->windows.offset=0;
	master->windows.cycling=False;
	//grabKey(master->id, CYCLE_WINDOWS_END_KEYCODE, XIAnyModifier,False);

}
Bool addWindow(Master *master,Window id){

	Window windowToInsert=id;
	Bool updated=False;

	if(id==master->windows.windowOrder[0])
		return False;
	assert(id!=0);

	for(int i=0; i<NUMBER_OF_WINDOWS; i++){
		Window temp=master->windows.windowOrder[i];
		master->windows.windowOrder[i]=windowToInsert;
		updated=True;
		windowToInsert=temp;
		if(temp==0){
			master->windows.numberOfWindows++;
		}
		if(temp==0||temp==id){
			break;
		}
	}


	return updated;

}
void dump(Master *master){
	printf("Dumping; current offset %d\n",master->windows.offset);
	for(int i=0; i<NUMBER_OF_WINDOWS&&master->windows.windowOrder[i]!=0; i++)
		printf("%ld ",master->windows.windowOrder[i]);
	printf("\nEnd dump\n");
}
