This information applies to anyone progamming their own application who would like to be able to control Audiosurf from it.

With the latest beta patch, you can control Audiosurf from an external app by sending WM_COPYDATA messages to the Audisourf window.

For c/c++, you can send a WM_COPYDATA with something like this:

```cpp
//get Audiosurf's window handle
HWND hwndTargetWin = FindWindow(NULL,"Audiosurf");

//create the command message and data struct
char* str = "asconfig freerideautoadvance false";
COPYDATASTRUCT cds;
cds.cbData = strlen(str)+1;
cds.lpData = (void*)str;

SendMessage(hwndTargetWin, WM_COPYDATA, 0, (LPARAM)&cds);
```

Audiosurf can receive commands once it has gotten to the character selection screen. There's also quickstart commands (explained below) that can be sent while Audiosurf is loading.

COMMANDS:
ascommand reloadtextures
ascommand reloadsounds
ascommand playsongcurrentcharacter c:\my music\nin\the way out is through.mp3
ascommand playsongmono c:\music\somesong.mp3
ascommand playsongpointman c:\music\somesong.mp3
ascommand playsongdoublevision c:\music\somesong.mp3
ascommand playsongmonopro c:\music\somesong.mp3
ascommand playsongvegas c:\music\somesong.mp3
ascommand playsongeraser c:\music\somesong.mp3
ascommand playsongpointmanpro c:\music\somesong.mp3
ascommand playsongpusher c:\music\somesong.mp3
ascommand playsongdoublevisionpro c:\music\somesong.mp3
ascommand playsongninjamono c:\music\somesong.mp3
ascommand playsongeraserelite c:\music\somesong.mp3
ascommand playsongpointmanelite c:\music\somesong.mp3
ascommand playsongpusherelite c:\music\somesong.mp3
ascommand playsongdoublevisionelite c:\music\somesong.mp3
ascommand playsongfreeride d:\music\09 Eternal Life.wma
ascommand setwindowpositionsize 100,100,640,480
ascommand setwindowalwaysontop
ascommand setwindowalwaysontopnoborder
ascommand runwithoutfocus (if you set the window as always on top, this gets set automatically)
ascommand closeaudiosurf
ascommand gotocharacterscreen
ascommand minimize
ascommand maximize
ascommand gofullscreen
ascommand restorenormalwindowstyle (puts the window back to default style and cancels "runwithoutfocus")




In addition to the command strings, you can configure some of Audiosurf's settings.
CONFIGS:
asconfig roadvisible true
asconfig sidewinder false
asconfig freerideblocks true
asconfig freeridecaterpillars false
asconfig freerideautoadvance true
asconfig showsongname true
asconfig usebankingcamera false




WINDOW REGISTRATION:
You can also send a command to register your window with Audiosurf:
ascommand registerlistenerwindow MyWindowTitle

Once you've successfully registered your window it will return a WM_COPYDATA message to you:
asreport successfullyregistered
And it will send messages on events:
REPORTS:
--When a song completes it sends a report with their score:
asreport songcomplete 142798
--Each time Audiosurf goes to the character selection screen it will send:
asreport oncharacterscreen

--Each time a song start playing it sends three commands:
asreport nowplayingartistname nine inch nails
--Followed by:
asreport nowplayingsongtitle just like you imagined
--Followed by:
asreport nowplayingashfile C:\\Audiosurf\\AudiosurfHC\\104415601 - SomeSong.mp3.ash




QUICKSTART COMMANDS:
There's two commands it will accept only while loading. Sending either one puts Audiosurf in quickstart mode where it will progress to the character selection screen with no clicks required.
1) ascommand quickstartregisterwindow MyWindowName
--It'll send you a "asreport successfullyquickstartregistered" to confirm registration
--Then it will send "asreport oncharacterscreen" when ready for further commands
--You don't need to register again to get messages usually sent to a registered window
2) ascommand quickstartqueuecommand CommandToRunWhenDoneLoadingGoesHere
--Example: ascommand quickstartqueuecommand ascommand playsongfreeride c:\music\some song.mp3
--Once it gets to the character selection screen it will run CommandToRunWhenDoneLoadingGoesHere




Note: Audiosurf will queue WM_COPYDATA messages if it receives more than one in a frame.


For Python, there's some info about sending and receiving WM_COPYDATA messages here (I haven't tried using Python for this yet):
http://blog.buffis.com/?p=29

For c/c++, you can send a WM_COPYDATA with something like this:
//get Audiosurf's window handle
HWND hwndTargetWin = FindWindow(NULL,"Audiosurf");

//create the command message and data struct
char* str = "asconfig freerideautoadvance false";
COPYDATASTRUCT cds;
cds.cbData = strlen(str)+1;
cds.lpData = (void*)str;

SendMessage(hwndTargetWin, WM_COPYDATA, 0, (LPARAM)&cds);