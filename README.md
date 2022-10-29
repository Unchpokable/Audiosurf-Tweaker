# Audiosurf Tweaker
---
## What is it?
- Audiosurf Twekaer - A tool for managing Audiosurf textures and share texture packs between people. Also, tweaker can interact with audiosurf to use some uncommon configurations

---
## Installation guide 
### If you downloading Audiosurf Tweaker first time:
- Download latest Release witn -Full tag from [releases page](https://github.com/Unchpokable/Audiosurf-SkinChanger/releases)
- Extract all files from downloaded release's .zip to any folder you want
- Run "tweaker.exe"
- Enjoy

### If you want to update your old Tweaker:
- Download Update.zip from latest release from [releases page](https://github.com/Unchpokable/Audiosurf-SkinChanger/releases)
- replace your old Audiosurf Skin Changer files by new that contains in downloaded .zip
- If your old Audiosurf Skin Changer version incompatible with latest release, use instructions above

## Userguide
### Program aparted into 3 tabs. You can see them in left menu:
  - Skin Changer 
  - Command and Control
  - Settings
  
 ![left menu](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/Beta-Alt-Design/Docs/readme/left_menu.png "Left menu")
  
### Skin Changer Tab
![SkinChanger](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/Beta-Alt-Design/Docs/readme/SkinChangerTab.png "Skin Changer")
  - The tab name says it all. From here you can manage your skins and game' textures: browse, install, edit, etc.
  - In the top of the tab placed list with all skins that loaded by Tweaker:
 ![skins list](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/Beta-Alt-Design/Docs/readme/SkinsList.png "Skins selection")
  - When you hover mouse above list item, it will show you skin preview
  - To install skin that you want, you can press "Install" button on the left of the skin item.
  - On the right of the skin item you can see 4 buttons: "Export", "Rename", "Edit" and "Remove"
    * Export: Tweaker will create copy of skin that you've selected in directory that you've selected
    * Rename: Change displayed skin name
    * Edit: Unpack skin content into temporal folder and edit it
    * Remove: remove
  - If you wand to install only some parts of some skin or combine some skins, See "Custom Installation" in the bottom of the tab:
  ![Custom Installation](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/Beta-Alt-Design/Docs/readme/CustomInstallationsScreenshots.png "Custom Installation")
  
    Here you can select what parts of skin you want and install only them ("Install Selected" button). Also, you can reset other uncheked game' textures into default clicking "Clear Install Selected".

  - Below skins selection list you cann see toolbar with 3 buttons: "Add New", "Export" and "Refresh"
  ![Changer Toolbar](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/Beta-Alt-Design/Docs/readme/SCToolbar.png "Toolbar")
    * "Add new" - Select skin package file in opened File Selection Dialog and Tweaker will automatically move it into root program skins folder and load it into selection list.
    * "Export" - export your current game' textures and save it into Tweaker Skin Package. Exported package will be loaded into selection list, selected and will be activated rename input textbox.
    * "Refresh" - clean up program cache and load all skins from root skins folder and additional skins folder again
    
### Command and Control tab
- From here you can send to audiosurf some configuration commands.
![Command and control](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/Beta-Alt-Design/Docs/readme/Commands.png "Tweaker")
  * Checkboxes in "Tweaks" section represents some game configs that have 2 state - enabled or disabled. This checkboxes will be active even if audiosurf is not connected and after game window will connect, Tweaker will send to it all config commands that accumulated in queue while you clicking this checkboxes without game connected.
  * Tweaks "Invisible Road", "Banking camera" and "Hidden song title" applies in real time, other need to you be in the game menu
  * Commands section will be inactive while game not connected, because most of this configurations targeted to game' main window parameters. Little thing - on the different game versions and different moon phases this commands works also different (say thanks to game' dev), so be careful with it. Sometimes some of them will crash game or cause it' undefined behaviour. Some - not. You will never know how this commands will work today 'till you try. It's like a russian roulette, enjoy :)

### Settings tab
![Settings](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/Beta-Alt-Design/Docs/readme/Settings.png "Settings")
- Path to Audiosurf textures means Path to Audiosurf textures. Simple. Tweaker can detect this path automatically. But, if you using non-steam version of game or your steamlibrary aparted with main steam install path, you need to set path to game' textures manualy. 
- Additional skins folder path - You can choose special folder that Tweaker will be scan for skins with root skins folder. 
- Skin Changer Settings:
  * Hot Reload - After skin installation Tweaker will send to Audiosurf command to reload textures so you can see changes even without retry song. Sadly, Audiosurf does not reloading skysphere so in real time updates only tiles, rings, hits and fireworks
  * Control game' textures folder content - Tweaker will track content in game' textures folder and warn you if it dont match with content that Tweaker remember.
  * Safety installation - Can be enabled only if enabled folder content control. Will block installation of skins while content in game' textures folder is unsaved by Tweaker.
  
### Other features
- Audiosurf Tweaker can find active game process automatically, but only if it has his default name "QuestViewer.exe" and there is a single process called like that in the system. If your Tweaker hangs on "Wait for AS Approve" even if game running on the character screen or even in track, it can be caused by Tweaker handled wrong process. In this case you can click "Find running Audiosurf" and in the opened window select your game process.
