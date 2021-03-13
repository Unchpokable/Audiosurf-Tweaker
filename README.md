# Audiosurf Skin Changer
---
## What is it?
- Audiosurf Skin Changer - A simple program that makes it easy to swap textures in Audiosurf and share texture packs between people
- The program uses special .askin files, which store information about which textures need to be replaced and new images for those textures.
- At the moment you can only get the .askin file from another Audiosurf Skin Changer user or use pre-installed skins.

---
## Installation guide
- Program uses special .askin files so it need to register this extension in Windows Registry so that you can see the custom icons for these files . For this purpose the installer.exe script is included in the archive with the program (Archives with "-Installer" in name). This script performs the registration of the extension, as well as unpacking the attached archive with the program itself.
- After unpacking the archive you downloaded from release page, move the archive content (or subfolder content, if archive contains folder called like "x32" or "x64" or something like this) to where you want to place program. Read the installation manual carefully, run the installer.exe and wait for it to finish. This completes the installation of the program, as you will be informed by the installer script. 


## Userguide
- The first thing you should do after starting the program for the first time is to check the path that is specified in the "Audiosurf Textures path" input field. Skin Changer can automatically detect Audiosurf textures folder, but, if u using illegal Audiosurf copy, this path can be invalid, cause its based on steam install path.
- Program has folder for skins in his root directory (%SkinChanger.exe directory%\Skins), but you can add additional folder which Skin Changer will scan for .askin skinpacks ("Path to skins" entry filed)
- To open downloaded .askin file, press "Add new .askin file" and select .askin that you want to install. New skin will be added to Skins list, but after restart program it will be deleted. To save skin, select it and press "Export as .askin" button. New .askin file will be created in folder that you select as additional skins folder or in default skins folder.
- You can pack your textures into .askin file and send it to your friend. Press "Add new skin from folder" button and select folder with textures. Programm will offer you to name new skin, if you deny this offer, skin will be automatically named as folder that you select. Make sure that all textures are named as they should be
- If you want to create new texturepack, but textures that you want to use placed in different folders, you can use "Skin creator" and pack all textures that you want into .askin texturepack
- The program only replaces textures that contain .askin, so it's okay if your .askin doesn't contain anything but, for example, tiles.png (one of audiosurf texutures pictures)
- To install texturepack, select it in ListBox and press "install selected" button
