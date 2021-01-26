namespace Audiosurf_SkinChanger
{
    partial class Form1
    {
        /// <summary>
        /// Обязательная переменная конструктора.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Освободить все используемые ресурсы.
        /// </summary>
        /// <param name="disposing">истинно, если управляемый ресурс должен быть удален; иначе ложно.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Код, автоматически созданный конструктором форм Windows

        /// <summary>
        /// Требуемый метод для поддержки конструктора — не изменяйте 
        /// содержимое этого метода с помощью редактора кода.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.label1 = new System.Windows.Forms.Label();
            this.pathToGameTextbox = new System.Windows.Forms.TextBox();
            this.viewPathToGameBtn = new System.Windows.Forms.Button();
            this.openSkinDialog = new System.Windows.Forms.OpenFileDialog();
            this.saveButton = new System.Windows.Forms.Button();
            this.previewGroupBox = new System.Windows.Forms.GroupBox();
            this.hitPic2 = new System.Windows.Forms.PictureBox();
            this.hitPic1 = new System.Windows.Forms.PictureBox();
            this.label3 = new System.Windows.Forms.Label();
            this.SkyspherePic3 = new System.Windows.Forms.PictureBox();
            this.SkyspherePic2 = new System.Windows.Forms.PictureBox();
            this.label5 = new System.Windows.Forms.Label();
            this.ringPic4 = new System.Windows.Forms.PictureBox();
            this.ringPic3 = new System.Windows.Forms.PictureBox();
            this.ringPic2 = new System.Windows.Forms.PictureBox();
            this.ringPic1 = new System.Windows.Forms.PictureBox();
            this.label4 = new System.Windows.Forms.Label();
            this.partPic3 = new System.Windows.Forms.PictureBox();
            this.partPic2 = new System.Windows.Forms.PictureBox();
            this.partPic1 = new System.Windows.Forms.PictureBox();
            this.particlesLabel = new System.Windows.Forms.Label();
            this.tileFlyup = new System.Windows.Forms.PictureBox();
            this.tilePic4 = new System.Windows.Forms.PictureBox();
            this.tilePic3 = new System.Windows.Forms.PictureBox();
            this.tilePic2 = new System.Windows.Forms.PictureBox();
            this.tilePic1 = new System.Windows.Forms.PictureBox();
            this.tilesLabel = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.skySpherePic = new System.Windows.Forms.PictureBox();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.cleanInstallCheck = new System.Windows.Forms.CheckBox();
            this.label6 = new System.Windows.Forms.Label();
            this.viewPathToSkinsBtn = new System.Windows.Forms.Button();
            this.skinsFolderPathTextbox = new System.Windows.Forms.TextBox();
            this.button1 = new System.Windows.Forms.Button();
            this.button4 = new System.Windows.Forms.Button();
            this.button3 = new System.Windows.Forms.Button();
            this.openSkinBtn = new System.Windows.Forms.Button();
            this.button2 = new System.Windows.Forms.Button();
            this.SkinsListBox = new System.Windows.Forms.ListBox();
            this.folderBrowserDialog1 = new System.Windows.Forms.FolderBrowserDialog();
            this.toolTip1 = new System.Windows.Forms.ToolTip(this.components);
            this.previewGroupBox.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.hitPic2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.hitPic1)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.SkyspherePic3)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.SkyspherePic2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.ringPic4)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.ringPic3)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.ringPic2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.ringPic1)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.partPic3)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.partPic2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.partPic1)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tileFlyup)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tilePic4)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tilePic3)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tilePic2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tilePic1)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.skySpherePic)).BeginInit();
            this.groupBox1.SuspendLayout();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(12, 367);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(131, 13);
            this.label1.TabIndex = 0;
            this.label1.Text = "Path to Audiosurf textures:";
            // 
            // pathToGameTextbox
            // 
            this.pathToGameTextbox.BackColor = System.Drawing.SystemColors.ControlDark;
            this.pathToGameTextbox.Location = new System.Drawing.Point(12, 383);
            this.pathToGameTextbox.Name = "pathToGameTextbox";
            this.pathToGameTextbox.Size = new System.Drawing.Size(140, 20);
            this.pathToGameTextbox.TabIndex = 1;
            // 
            // viewPathToGameBtn
            // 
            this.viewPathToGameBtn.Location = new System.Drawing.Point(158, 381);
            this.viewPathToGameBtn.Name = "viewPathToGameBtn";
            this.viewPathToGameBtn.Size = new System.Drawing.Size(75, 23);
            this.viewPathToGameBtn.TabIndex = 2;
            this.viewPathToGameBtn.Text = "View...";
            this.viewPathToGameBtn.UseVisualStyleBackColor = true;
            this.viewPathToGameBtn.Click += new System.EventHandler(this.ViewPathDialogShow);
            // 
            // openSkinDialog
            // 
            this.openSkinDialog.FileName = "openFileDialog1";
            // 
            // saveButton
            // 
            this.saveButton.Location = new System.Drawing.Point(12, 415);
            this.saveButton.Name = "saveButton";
            this.saveButton.Size = new System.Drawing.Size(221, 23);
            this.saveButton.TabIndex = 3;
            this.saveButton.Text = "Сохранить";
            this.saveButton.UseVisualStyleBackColor = true;
            this.saveButton.Click += new System.EventHandler(this.SavePathes);
            // 
            // previewGroupBox
            // 
            this.previewGroupBox.BackColor = System.Drawing.SystemColors.ControlDarkDark;
            this.previewGroupBox.Controls.Add(this.hitPic2);
            this.previewGroupBox.Controls.Add(this.hitPic1);
            this.previewGroupBox.Controls.Add(this.label3);
            this.previewGroupBox.Controls.Add(this.SkyspherePic3);
            this.previewGroupBox.Controls.Add(this.SkyspherePic2);
            this.previewGroupBox.Controls.Add(this.label5);
            this.previewGroupBox.Controls.Add(this.ringPic4);
            this.previewGroupBox.Controls.Add(this.ringPic3);
            this.previewGroupBox.Controls.Add(this.ringPic2);
            this.previewGroupBox.Controls.Add(this.ringPic1);
            this.previewGroupBox.Controls.Add(this.label4);
            this.previewGroupBox.Controls.Add(this.partPic3);
            this.previewGroupBox.Controls.Add(this.partPic2);
            this.previewGroupBox.Controls.Add(this.partPic1);
            this.previewGroupBox.Controls.Add(this.particlesLabel);
            this.previewGroupBox.Controls.Add(this.tileFlyup);
            this.previewGroupBox.Controls.Add(this.tilePic4);
            this.previewGroupBox.Controls.Add(this.tilePic3);
            this.previewGroupBox.Controls.Add(this.tilePic2);
            this.previewGroupBox.Controls.Add(this.tilePic1);
            this.previewGroupBox.Controls.Add(this.tilesLabel);
            this.previewGroupBox.Controls.Add(this.label2);
            this.previewGroupBox.Controls.Add(this.skySpherePic);
            this.previewGroupBox.Location = new System.Drawing.Point(239, 13);
            this.previewGroupBox.Name = "previewGroupBox";
            this.previewGroupBox.Size = new System.Drawing.Size(611, 425);
            this.previewGroupBox.TabIndex = 4;
            this.previewGroupBox.TabStop = false;
            this.previewGroupBox.Text = "Preview";
            // 
            // hitPic2
            // 
            this.hitPic2.Location = new System.Drawing.Point(225, 316);
            this.hitPic2.Name = "hitPic2";
            this.hitPic2.Size = new System.Drawing.Size(64, 64);
            this.hitPic2.TabIndex = 27;
            this.hitPic2.TabStop = false;
            // 
            // hitPic1
            // 
            this.hitPic1.Location = new System.Drawing.Point(126, 316);
            this.hitPic1.Name = "hitPic1";
            this.hitPic1.Size = new System.Drawing.Size(64, 64);
            this.hitPic1.TabIndex = 26;
            this.hitPic1.TabStop = false;
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(6, 316);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(28, 13);
            this.label3.TabIndex = 25;
            this.label3.Text = "Hits:";
            // 
            // SkyspherePic3
            // 
            this.SkyspherePic3.Location = new System.Drawing.Point(445, 19);
            this.SkyspherePic3.Name = "SkyspherePic3";
            this.SkyspherePic3.Size = new System.Drawing.Size(160, 80);
            this.SkyspherePic3.TabIndex = 24;
            this.SkyspherePic3.TabStop = false;
            // 
            // SkyspherePic2
            // 
            this.SkyspherePic2.Location = new System.Drawing.Point(285, 19);
            this.SkyspherePic2.Name = "SkyspherePic2";
            this.SkyspherePic2.Size = new System.Drawing.Size(160, 80);
            this.SkyspherePic2.TabIndex = 23;
            this.SkyspherePic2.TabStop = false;
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(296, 19);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(0, 13);
            this.label5.TabIndex = 20;
            // 
            // ringPic4
            // 
            this.ringPic4.Location = new System.Drawing.Point(417, 245);
            this.ringPic4.Name = "ringPic4";
            this.ringPic4.Size = new System.Drawing.Size(64, 64);
            this.ringPic4.TabIndex = 19;
            this.ringPic4.TabStop = false;
            // 
            // ringPic3
            // 
            this.ringPic3.Location = new System.Drawing.Point(321, 245);
            this.ringPic3.Name = "ringPic3";
            this.ringPic3.Size = new System.Drawing.Size(64, 64);
            this.ringPic3.TabIndex = 18;
            this.ringPic3.TabStop = false;
            // 
            // ringPic2
            // 
            this.ringPic2.Location = new System.Drawing.Point(225, 245);
            this.ringPic2.Name = "ringPic2";
            this.ringPic2.Size = new System.Drawing.Size(64, 64);
            this.ringPic2.TabIndex = 17;
            this.ringPic2.TabStop = false;
            // 
            // ringPic1
            // 
            this.ringPic1.Location = new System.Drawing.Point(126, 245);
            this.ringPic1.Name = "ringPic1";
            this.ringPic1.Size = new System.Drawing.Size(64, 64);
            this.ringPic1.TabIndex = 16;
            this.ringPic1.TabStop = false;
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(6, 245);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(37, 13);
            this.label4.TabIndex = 15;
            this.label4.Text = "Rings:";
            // 
            // partPic3
            // 
            this.partPic3.Location = new System.Drawing.Point(321, 175);
            this.partPic3.Name = "partPic3";
            this.partPic3.Size = new System.Drawing.Size(64, 64);
            this.partPic3.TabIndex = 14;
            this.partPic3.TabStop = false;
            // 
            // partPic2
            // 
            this.partPic2.Location = new System.Drawing.Point(225, 175);
            this.partPic2.Name = "partPic2";
            this.partPic2.Size = new System.Drawing.Size(64, 64);
            this.partPic2.TabIndex = 13;
            this.partPic2.TabStop = false;
            // 
            // partPic1
            // 
            this.partPic1.Location = new System.Drawing.Point(126, 175);
            this.partPic1.Name = "partPic1";
            this.partPic1.Size = new System.Drawing.Size(64, 64);
            this.partPic1.TabIndex = 12;
            this.partPic1.TabStop = false;
            // 
            // particlesLabel
            // 
            this.particlesLabel.AutoSize = true;
            this.particlesLabel.Location = new System.Drawing.Point(6, 175);
            this.particlesLabel.Name = "particlesLabel";
            this.particlesLabel.Size = new System.Drawing.Size(50, 13);
            this.particlesLabel.TabIndex = 10;
            this.particlesLabel.Text = "Particles:";
            // 
            // tileFlyup
            // 
            this.tileFlyup.Location = new System.Drawing.Point(512, 105);
            this.tileFlyup.Name = "tileFlyup";
            this.tileFlyup.Size = new System.Drawing.Size(64, 64);
            this.tileFlyup.TabIndex = 7;
            this.tileFlyup.TabStop = false;
            // 
            // tilePic4
            // 
            this.tilePic4.Location = new System.Drawing.Point(417, 105);
            this.tilePic4.Name = "tilePic4";
            this.tilePic4.Size = new System.Drawing.Size(64, 64);
            this.tilePic4.TabIndex = 6;
            this.tilePic4.TabStop = false;
            // 
            // tilePic3
            // 
            this.tilePic3.Location = new System.Drawing.Point(321, 105);
            this.tilePic3.Name = "tilePic3";
            this.tilePic3.Size = new System.Drawing.Size(64, 64);
            this.tilePic3.TabIndex = 5;
            this.tilePic3.TabStop = false;
            // 
            // tilePic2
            // 
            this.tilePic2.Location = new System.Drawing.Point(225, 105);
            this.tilePic2.Name = "tilePic2";
            this.tilePic2.Size = new System.Drawing.Size(64, 64);
            this.tilePic2.TabIndex = 4;
            this.tilePic2.TabStop = false;
            // 
            // tilePic1
            // 
            this.tilePic1.Location = new System.Drawing.Point(126, 105);
            this.tilePic1.Name = "tilePic1";
            this.tilePic1.Size = new System.Drawing.Size(64, 64);
            this.tilePic1.TabIndex = 3;
            this.tilePic1.TabStop = false;
            // 
            // tilesLabel
            // 
            this.tilesLabel.AutoSize = true;
            this.tilesLabel.Location = new System.Drawing.Point(6, 105);
            this.tilesLabel.Name = "tilesLabel";
            this.tilesLabel.Size = new System.Drawing.Size(32, 13);
            this.tilesLabel.TabIndex = 2;
            this.tilesLabel.Text = "Tiles:";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(6, 19);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(70, 13);
            this.label2.TabIndex = 1;
            this.label2.Text = "Sky Spheres:";
            // 
            // skySpherePic
            // 
            this.skySpherePic.Location = new System.Drawing.Point(126, 19);
            this.skySpherePic.Name = "skySpherePic";
            this.skySpherePic.Size = new System.Drawing.Size(160, 80);
            this.skySpherePic.TabIndex = 0;
            this.skySpherePic.TabStop = false;
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.cleanInstallCheck);
            this.groupBox1.Controls.Add(this.label6);
            this.groupBox1.Controls.Add(this.viewPathToSkinsBtn);
            this.groupBox1.Controls.Add(this.skinsFolderPathTextbox);
            this.groupBox1.Controls.Add(this.button1);
            this.groupBox1.Controls.Add(this.button4);
            this.groupBox1.Controls.Add(this.button3);
            this.groupBox1.Controls.Add(this.openSkinBtn);
            this.groupBox1.Controls.Add(this.button2);
            this.groupBox1.Controls.Add(this.SkinsListBox);
            this.groupBox1.Location = new System.Drawing.Point(12, 13);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(221, 351);
            this.groupBox1.TabIndex = 5;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Your skins";
            // 
            // cleanInstallCheck
            // 
            this.cleanInstallCheck.AutoSize = true;
            this.cleanInstallCheck.Location = new System.Drawing.Point(6, 18);
            this.cleanInstallCheck.Name = "cleanInstallCheck";
            this.cleanInstallCheck.Size = new System.Drawing.Size(105, 17);
            this.cleanInstallCheck.TabIndex = 8;
            this.cleanInstallCheck.Text = "Clean installation";
            this.cleanInstallCheck.UseVisualStyleBackColor = true;
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(6, 37);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(73, 13);
            this.label6.TabIndex = 7;
            this.label6.Text = "Path to Skins:";
            // 
            // viewPathToSkinsBtn
            // 
            this.viewPathToSkinsBtn.Location = new System.Drawing.Point(146, 51);
            this.viewPathToSkinsBtn.Name = "viewPathToSkinsBtn";
            this.viewPathToSkinsBtn.Size = new System.Drawing.Size(69, 23);
            this.viewPathToSkinsBtn.TabIndex = 6;
            this.viewPathToSkinsBtn.Text = "View...";
            this.viewPathToSkinsBtn.UseVisualStyleBackColor = true;
            this.viewPathToSkinsBtn.Click += new System.EventHandler(this.ViewPathDialogShow);
            // 
            // skinsFolderPathTextbox
            // 
            this.skinsFolderPathTextbox.Location = new System.Drawing.Point(7, 53);
            this.skinsFolderPathTextbox.Name = "skinsFolderPathTextbox";
            this.skinsFolderPathTextbox.Size = new System.Drawing.Size(133, 20);
            this.skinsFolderPathTextbox.TabIndex = 6;
            // 
            // button1
            // 
            this.button1.Location = new System.Drawing.Point(7, 235);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(210, 23);
            this.button1.TabIndex = 5;
            this.button1.Text = "Add new skin from .zip";
            this.button1.UseVisualStyleBackColor = true;
            this.button1.Click += new System.EventHandler(this.OpenSkinFromZip);
            // 
            // button4
            // 
            this.button4.Location = new System.Drawing.Point(6, 322);
            this.button4.Name = "button4";
            this.button4.Size = new System.Drawing.Size(210, 23);
            this.button4.TabIndex = 4;
            this.button4.Text = "Export selected as .askin";
            this.button4.UseVisualStyleBackColor = true;
            this.button4.Click += new System.EventHandler(this.PackToSkinFile);
            // 
            // button3
            // 
            this.button3.Location = new System.Drawing.Point(7, 264);
            this.button3.Name = "button3";
            this.button3.Size = new System.Drawing.Size(211, 23);
            this.button3.TabIndex = 3;
            this.button3.Text = "Add new skin from folder";
            this.button3.UseVisualStyleBackColor = true;
            this.button3.Click += new System.EventHandler(this.PackFolderIntoSkin);
            // 
            // openSkinBtn
            // 
            this.openSkinBtn.Location = new System.Drawing.Point(7, 206);
            this.openSkinBtn.Name = "openSkinBtn";
            this.openSkinBtn.Size = new System.Drawing.Size(211, 23);
            this.openSkinBtn.TabIndex = 2;
            this.openSkinBtn.Text = "Add new .askin";
            this.openSkinBtn.UseVisualStyleBackColor = true;
            this.openSkinBtn.Click += new System.EventHandler(this.OpenSkinBtnClick);
            // 
            // button2
            // 
            this.button2.Location = new System.Drawing.Point(7, 293);
            this.button2.Name = "button2";
            this.button2.Size = new System.Drawing.Size(210, 23);
            this.button2.TabIndex = 1;
            this.button2.Text = "Install selected";
            this.button2.UseVisualStyleBackColor = true;
            this.button2.Click += new System.EventHandler(this.InstallSkin);
            // 
            // SkinsListBox
            // 
            this.SkinsListBox.BackColor = System.Drawing.SystemColors.ControlDark;
            this.SkinsListBox.FormattingEnabled = true;
            this.SkinsListBox.Location = new System.Drawing.Point(7, 79);
            this.SkinsListBox.Name = "SkinsListBox";
            this.SkinsListBox.Size = new System.Drawing.Size(209, 121);
            this.SkinsListBox.TabIndex = 0;
            this.SkinsListBox.SelectedIndexChanged += new System.EventHandler(this.SkinsListBox_SelectedIndexChanged);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.SystemColors.ControlDarkDark;
            this.ClientSize = new System.Drawing.Size(862, 447);
            this.Controls.Add(this.groupBox1);
            this.Controls.Add(this.previewGroupBox);
            this.Controls.Add(this.saveButton);
            this.Controls.Add(this.viewPathToGameBtn);
            this.Controls.Add(this.pathToGameTextbox);
            this.Controls.Add(this.label1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "Form1";
            this.Text = "Audiosurf Skin Changer";
            this.previewGroupBox.ResumeLayout(false);
            this.previewGroupBox.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.hitPic2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.hitPic1)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.SkyspherePic3)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.SkyspherePic2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.ringPic4)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.ringPic3)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.ringPic2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.ringPic1)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.partPic3)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.partPic2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.partPic1)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tileFlyup)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tilePic4)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tilePic3)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tilePic2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tilePic1)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.skySpherePic)).EndInit();
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox pathToGameTextbox;
        private System.Windows.Forms.Button viewPathToGameBtn;
        private System.Windows.Forms.OpenFileDialog openSkinDialog;
        private System.Windows.Forms.Button saveButton;
        private System.Windows.Forms.GroupBox previewGroupBox;
        private System.Windows.Forms.PictureBox tilePic1;
        private System.Windows.Forms.Label tilesLabel;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.PictureBox skySpherePic;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.PictureBox ringPic4;
        private System.Windows.Forms.PictureBox ringPic3;
        private System.Windows.Forms.PictureBox ringPic2;
        private System.Windows.Forms.PictureBox ringPic1;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.PictureBox partPic3;
        private System.Windows.Forms.PictureBox partPic2;
        private System.Windows.Forms.PictureBox partPic1;
        private System.Windows.Forms.Label particlesLabel;
        private System.Windows.Forms.PictureBox tileFlyup;
        private System.Windows.Forms.PictureBox tilePic4;
        private System.Windows.Forms.PictureBox tilePic3;
        private System.Windows.Forms.PictureBox tilePic2;
        private System.Windows.Forms.FolderBrowserDialog folderBrowserDialog1;
        private System.Windows.Forms.ListBox SkinsListBox;
        private System.Windows.Forms.Button openSkinBtn;
        private System.Windows.Forms.Button button2;
        private System.Windows.Forms.PictureBox SkyspherePic3;
        private System.Windows.Forms.PictureBox SkyspherePic2;
        private System.Windows.Forms.Button button3;
        private System.Windows.Forms.Button button4;
        private System.Windows.Forms.PictureBox hitPic2;
        private System.Windows.Forms.PictureBox hitPic1;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Button button1;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.Button viewPathToSkinsBtn;
        private System.Windows.Forms.TextBox skinsFolderPathTextbox;
        private System.Windows.Forms.CheckBox cleanInstallCheck;
        private System.Windows.Forms.ToolTip toolTip1;
    }
}

