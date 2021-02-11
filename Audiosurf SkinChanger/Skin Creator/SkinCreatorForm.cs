using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Audiosurf_SkinChanger.Skin_Creator
{
    public partial class SkinCreatorForm : Form
    {
        private enum States
        {
            Idle,
            Active
        }

        private Dictionary<string, PictureBox> AssotiateTable;
        private Dictionary<string, Dictionary<States, Bitmap>> StatesTable;
        private Dictionary<string, ImageInfo> ImageAssociationTable;
        private Dictionary<PictureBox, Utilities.ImageGroup> SkinImageGroupAssociationTable;
        private Dictionary<PictureBox, Engine.NamedBitmap> SkinBitmapsAssociationTable;
        private Engine.NamedBitmap TilesTemp;
        private Engine.AudiosurfSkin Skin;

        public SkinCreatorForm()
        {
            InitializeComponent();
            LoadIdleImages();
            CreateAssotiativeTable();
            CreateStateAssotiatieTable();
            CreateImageAssociationTable();
            Skin = new Engine.AudiosurfSkin();
            CreateSkinFieldsAssotiationTables();
        }

        private void CreateAssotiativeTable()
        {
            AssotiateTable = new Dictionary<string, PictureBox>()
            {
                { "Sphere1", Sphere1 },
                { "Sphere2", Sphere2 },
                { "Sphere3", Sphere3 },
                { "tile1", tile1 },
                {"tile2", tile2 },
                {"tile3", tile3 },
                {"tile4", tile4 },
                {"tileflyup", tileflyup },
                {"part1", part1},
                {"part2", part2 },
                {"part3", part3 },
                {"ring1", ring1 },
                {"ring2", ring2 },
                {"ring3", ring3 },
                {"ring4", ring4 },
                {"hit1", hit1 },
                {"hit2", hit2 }
            };
        }

        private void CreateStateAssotiatieTable()
        {
            StatesTable = new Dictionary<string, Dictionary<States, Bitmap>>
            {
                { "Sphere1", new Dictionary<States, Bitmap>() { {States.Active, Images.Add_SkySphere_Active }, { States.Idle, Images.Add_Skysphere} } },
                { "Sphere2", new Dictionary<States, Bitmap>() { {States.Active, Images.Add_SkySphere_Active }, { States.Idle, Images.Add_Skysphere} } },
                { "Sphere3", new Dictionary<States, Bitmap>() { {States.Active, Images.Add_SkySphere_Active }, { States.Idle, Images.Add_Skysphere} } },
                { "tile1", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Tiles_Active }, { States.Idle, Images.Add_Tiles} } },
                {"tile2", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Tiles_Active }, { States.Idle, Images.Add_Tiles } } },
                {"tile3", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Tiles_Active }, { States.Idle, Images.Add_Tiles } } },
                {"tile4", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Tiles_Active }, { States.Idle, Images.Add_Tiles } } },
                {"tileflyup", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Tile_Flyup_Active }, { States.Idle, Images.Add_TileFlyup } } },
                {"part1", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Particles1_Active }, { States.Idle, Images.Add_Particles1 } } },
                {"part2", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Particles2_Active }, { States.Idle, Images.Add_Particles2 } } },
                {"part3", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Particles3_Active }, { States.Idle, Images.Add_Particles3 } } },
                {"ring1", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Ring1A_Active }, { States.Idle, Images.Add_Ring1A } } },
                {"ring2", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Ring2A_Active }, { States.Idle, Images.Add_Ring2A } } },
                {"ring3", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Ring1B_Active }, { States.Idle, Images.Add_Ring1B } } },
                {"ring4", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Ring2B_Active }, { States.Idle, Images.Add_Ring2B } } },
                {"hit1", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Hit1_Active }, { States.Idle, Images.Add_Hit1 } } },
                {"hit2", new Dictionary<States, Bitmap>() { { States.Active, Images.Add_Hit2_Active }, { States.Idle, Images.Add_Hit2 } } }
            };
        }

        private void CreateImageAssociationTable()
        {
            ImageAssociationTable = new Dictionary<string, ImageInfo>()
            {
                { "Sphere1", new ImageInfo("png", "Skysphere_White.png") },
                { "Sphere2", new ImageInfo("png", "Skysphere_Black.png") },
                { "Sphere3", new ImageInfo("png", "Skysphere_Grey.png") },
                { "tile1", new ImageInfo("png", "tiles.png") },
                {"tile2", new ImageInfo("png", "tiles.png") },
                {"tile3", new ImageInfo("png", "tiles.png") },
                {"tile4", new ImageInfo("png", "tiles.png") },
                {"tileflyup", new ImageInfo("png", "tileflyup.png") },
                {"part1", new ImageInfo("png", "particles1.png")},
                {"part2", new ImageInfo("jpg", "particles2.jpg") },
                {"part3", new ImageInfo("jpg", "particles3.jpg") },
                {"ring1", new ImageInfo("png", "ring1A.png") },
                {"ring2", new ImageInfo("jpg", "ring2A.jpg") },
                {"ring3", new ImageInfo("png", "ring1B.png") },
                {"ring4", new ImageInfo("jpg", "ring2B.jpg") },
                {"hit1", new ImageInfo("png", "hit1.png") },
                {"hit2", new ImageInfo("jpg", "hit2.jpg") }
            };
        }

        private void CreateSkinFieldsAssotiationTables()
        {
            SkinBitmapsAssociationTable = new Dictionary<PictureBox, Engine.NamedBitmap>()
            {
                {tileflyup, Skin.TilesFlyup }
            };


            SkinImageGroupAssociationTable = new Dictionary<PictureBox, Utilities.ImageGroup>()
            {
                {Sphere1, Skin.SkySpheres },
                {Sphere2, Skin.SkySpheres },
                {Sphere3, Skin.SkySpheres },

                {part1, Skin.Particles },
                {part2, Skin.Particles },
                {part3, Skin.Particles },

                {ring1, Skin.Particles },
                {ring2, Skin.Particles },
                {ring3, Skin.Particles },
                {ring4, Skin.Particles },

                {hit1, Skin.Particles },
                {hit2, Skin.Particles },
            };
        }


        private void LoadIdleImages()
        {
            Sphere1.Image = Images.Add_Skysphere;
            Sphere2.Image = Images.Add_Skysphere;
            Sphere3.Image = Images.Add_Skysphere;

            tile1.Image = Images.Add_Tiles;
            tile2.Image = Images.Add_Tiles;
            tile3.Image = Images.Add_Tiles;
            tile4.Image = Images.Add_Tiles;

            tileflyup.Image = Images.Add_TileFlyup;

            part1.Image = Images.Add_Particles1;
            part2.Image = Images.Add_Particles2;
            part3.Image = Images.Add_Particles3;

            ring1.Image = Images.Add_Ring1A;
            ring2.Image = Images.Add_Ring2A;
            ring3.Image = Images.Add_Ring1B;
            ring4.Image = Images.Add_Ring2B;

            hit1.Image = Images.Add_Hit1;
            hit2.Image = Images.Add_Hit2;
        }

        private void SetActiveSphere(object sender, EventArgs e)
        {
            var knownSender = sender as PictureBox;
            AssotiateTable[knownSender.Name].Image = StatesTable[knownSender.Name][States.Active];
        }

        private void Sphere1_MouseLeave(object sender, EventArgs e)
        {
            var knownSender = sender as PictureBox;
            AssotiateTable[knownSender.Name].Image = StatesTable[knownSender.Name][States.Idle];
        }

        private void FillImageSlot()
        {
            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {

            }
        }
    }
}
