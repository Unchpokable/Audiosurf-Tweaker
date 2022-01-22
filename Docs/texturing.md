# HOW TO: MAKING TEXTURE PACKS FOR AUDIOSURF #
* First of all, you need a graphics editor. Any of existing that supports PNG transparency. I'm using Adobe Photoshop CC 2021, but you free to use any other editor, even MS Paint 3D (Not common MS Paint. This editor doesn't supports transparency)
* Be sure that your game version supports texture editing. Its easy to check, just go to game root directory, find "engine" folder and look for "textures" folder. If this folder exists, your game supports texture editing
## Ok, i found "textures" folder. There is a couple of strange pictures. What should i do with it? ##
### So, let's check what we have
* First textures group is a pics called cliff-x.png 
    * ![Cliffs textures](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/master/Docs/images/cliffs.png "default cliff texture")
    * This textures used by game to texturing road cliff and some of characters. I highly recomment dont touch it, cause this textures applies pretty strange and there is no way to be 100% sure what happens with them in game
* Second group texture is a hits (hit1.png, hit2.jpg)
    * ![Hits textures](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/master/Docs/images/hits.png "default hits")
    * You can see this textures in game when you pick up any block. "hit1.png" used when you play with White background, hit2.jpg - when you play with Grey or Black backgrounds.
    * hit1.png as a texture for white background should be a .png image and should have transparent background
    * hit2.jpg as a texture for black/grey backgrounds should be a .jpg image and should have ABSOLUTE black background (hex #000000)
    * By default, this textures has 64x64 px resolution, but you free to upscale it as you want. At least, i've drawed texture packs with 512x512 hit textures and it works very good
    * You can color this textures, but remember that game colors it too, and overlaying your and game' color might look not as good as you imagine, so for the first time a good idea was a draw clear white textures, maybe with greyscale shadows
* Third group is a particles (particles1.png, particles2\3.jpg)
    * ![Particles Textures](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/master/Docs/images/particles.png "default particles")
    * This textures used by game to draw a fireworks on background when you playing. particles1.png used for puzzle modes with White background, particles2.jpg used for puzzle modes with Black\Grey background, particles3.jpg used for mono in any case
    * particles1.png should be a png.image with transparent background
    * particels2.jpg and particles3.jpg should be a .jpg image with absolute black background (hex #000000)
    * By default, this texture has a 128x128 px resulution, but as any other texture, you free to upscale it as you want
    * Particles texture aparted by 4 parts, and for fireworks game randomly select one of texture part as shown at image below
    * ![Particles cutting](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/master/Docs/images/particles_cutting.png "enumerated texture blocks with guiding lines")
    * I highly recommend using guiding lines in your graphics editor when you drawing this texture. Crookedly placed fireworks particles will be cutted by game and looks awful
* Fourth group is a rings (ringA\B.png\jpg)
    * ![Rings Textures](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/master/Docs/images/rings.png "rings")
    * This texture used by game to draw a tunnels on your track. Textures "ring1A.png", "ring2A.jpg" used by draw rings on track when you play with any graphics setting lower than maximum. Rings "B" used when you play on ultra hight graphics settings
    * As previous, .png textures used for white background, .jpg - for black and grey. Also as previous, .png textures should have transparent background, .jpg - absolute black (hex #000000)
    * Theoretically, Rings "A" should have a half resolution of "B" rings. By default, its 128x128 for "A" rings and 256x256 for "B" rings. But, now is 2022 year and any graphics card can play audiosurf on ultra high graphics with FPS that allows your monitor refresh rate, so "A" rings can be just copy of "B" rings for the game can load it without any problems.
    * As previous textures, you can upscale this texture as you want and color it, but i recommend to color rings only if you play with grey background everytime. In this case, game will not color rings by theyself and you'll see rings as colored as you want
    * If you playing with Black or White background, i recommend to use white and grey colors for drawing rings. If you understand what you do, or you're a graphics designer and can forsee, how some of color will be overlayed, you can use half-transparency with color what you want to get a new interesting look for your rings in the game
* Fifth goup is a tiles (tiles.png, tileflyup.png)
    * ![Tiles Textures](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/master/Docs/images/tiles.png "tiles")
    * This is a combined texture, that used by game to texturing blocks on your board. For the next explanation, we need to this scheme:
    * ![Tiles Textures](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/master/Docs/images/tiles_cutting.png "tiles")
    * Texture part marked 1 used to draw empty cells on your board. Remember that block "burn" timeout drawes also by this textures so if this texture will be very detalized, it can overlap texture of filled cell and can looks bad
    * Texture part marked 2 used to draw filled cell on your board. This is a texture that you'll see most of the play time. 
    * Texture part marked 3 used to draw filled cells in overfilled column. Be sure that overfill signal on this texture clearly see and understandable, othervise you risk not noticed overfill
    * Texture part marked 4 used to draw cracked grey blocks in mono mode. I had nothing to advice, draw here whatever you want
    * Tileflyup.png used for drawing cells that "burns", turns into points, and "fly" from your board to score counter at the top of the screen. I recomment use here same texture that you're uses for empty cell, but sometimes draw something else can look much better. 
 
## Problems that you can face with ##
* Sometimes texture "tileflyup.png" saved by Adobe Photoshop, even with right parameters (.png with transparent background), can turn into colored rectangle in game. Idk why this happening, and i guess even Dylan (game developer) dont know too. Simplest way to fix that problem - resave this image using any other graphics editor. 

## Using Audiosurf Skin Changer ##
* When you draw your texture pack, you can save it in Audiosurf Skin Changer for share it with other ASC users or just for yourself, if you had some different texture sets and want to easily swap it
* For export your texture set into ASC .askin file, you can use "Export my textures as .askin" or "Add new skin from folder" (see screenshot)
   * ![ASC](https://github.com/Unchpokable/Audiosurf-SkinChanger/blob/master/Docs/images/Audiosurf_SkinChanger_pFkao6HAUO.png "ASC")
   * "Export my textures as .askin" will export textures from your game' texture folder
   * "Add new skin from folder" will try to pack into AS skin conent of specified folder 
   * For more details about using ASC check userguide that placed in main repository page
