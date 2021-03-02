# Audiosurf Skin Changer
---
## What is it?
- Audiosurf Skin Changer - A simple program that makes it easy to swap textures in Audiosurf and share texture packs between people
- The program uses special .askin files, which store information about which textures need to be replaced and new images for those textures.
- At the moment you can only get the .askin file from another Audiosurf Skin Changer user or use pre-installed skins.

---
## Userguide
- The first thing you should do after starting the program for the first time is to check the path that is specified in the "Audiosurf Textures path" input field. Skin Changer can automatically detect Audiosurf textures folder, but, if u using illegal Audiosurf copy, this path can be invalid, cause its based on steam install path.
- Program has folder for skins in his root directory (%SkinChanger.exe directory%\Skins), but you can add additional folder which Skin Changer will scan for .askin skinpacks ("Path to skins" entry filed)
- To open downloaded .askin file, press "Add new .askin file" and select .askin that you want to install. New skin will be added to Skins list, but after restart program it will be deleted. To save skin, select it and press "Export as .askin" button. New .askin file will be created in folder that you select as additional skins folder or in default skins folder.
- You can pack your textures into .askin file and send it to your friend. Press "Add new skin from folder" button and select folder with textures. Programm will offer you to name new skin, if you deny this offer, skin will be automatically named as folder that you select. Make sure that all textures are named as they should be
- If you want to create new texturepack, but textures that you want to use placed in different folders, you can use "Skin creator" and pack all textures that you want into .askin texturepack
- The program only replaces textures that contain .askin, so it's okay if your .askin doesn't contain anything but, for example, tiles.png (one of audiosurf texutures pictures)
- To install texturepack, select it in ListBox and press "install selected" button

### About Skit creator
- Skin Creator - new feature in version 2.0 and as a consequence of not the best application architecture, this tool works with some hard-to-fix bugs for me. Specifically, because of the architecture of the ImageGroup type, which was not originally designed for such a tool, at the moment the texture replacement function almost does not work. As of the 2.0 release, it is recommended that you use this tool very carefully and watch what image you choose for a particular game texture.

