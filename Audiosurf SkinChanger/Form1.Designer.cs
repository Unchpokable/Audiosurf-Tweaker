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
            this.label1 = new System.Windows.Forms.Label();
            this.pathToGameTextbox = new System.Windows.Forms.TextBox();
            this.viewPathToGameBtn = new System.Windows.Forms.Button();
            this.openSkinDialog = new System.Windows.Forms.OpenFileDialog();
            this.saveButton = new System.Windows.Forms.Button();
            this.previewGroupBox = new System.Windows.Forms.GroupBox();
            this.SkyspherePic3 = new System.Windows.Forms.PictureBox();
            this.SkyspherePic2 = new System.Windows.Forms.PictureBox();
            this.label6 = new System.Windows.Forms.Label();
            this.button1 = new System.Windows.Forms.Button();
            this.label5 = new System.Windows.Forms.Label();
            this.ringPic4 = new System.Windows.Forms.PictureBox();
            this.ringPic3 = new System.Windows.Forms.PictureBox();
            this.ringPic2 = new System.Windows.Forms.PictureBox();
            this.ringPic1 = new System.Windows.Forms.PictureBox();
            this.label4 = new System.Windows.Forms.Label();
            this.partPic3 = new System.Windows.Forms.PictureBox();
            this.partPic2 = new System.Windows.Forms.PictureBox();
            this.partPic1 = new System.Windows.Forms.PictureBox();
            this.label3 = new System.Windows.Forms.Label();
            this.particlesLabel = new System.Windows.Forms.Label();
            this.skySphereCount = new System.Windows.Forms.Label();
            this.tilesPicCount = new System.Windows.Forms.Label();
            this.tileFlyup = new System.Windows.Forms.PictureBox();
            this.tilePic4 = new System.Windows.Forms.PictureBox();
            this.tilePic3 = new System.Windows.Forms.PictureBox();
            this.tilePic2 = new System.Windows.Forms.PictureBox();
            this.tilePic1 = new System.Windows.Forms.PictureBox();
            this.tilesLabel = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.skySpherePic = new System.Windows.Forms.PictureBox();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.button4 = new System.Windows.Forms.Button();
            this.button3 = new System.Windows.Forms.Button();
            this.openSkinBtn = new System.Windows.Forms.Button();
            this.button2 = new System.Windows.Forms.Button();
            this.SkinsListBox = new System.Windows.Forms.ListBox();
            this.folderBrowserDialog1 = new System.Windows.Forms.FolderBrowserDialog();
            this.previewGroupBox.SuspendLayout();
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
            this.label1.Size = new System.Drawing.Size(69, 13);
            this.label1.TabIndex = 0;
            this.label1.Text = "Путь к игре:";
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
            this.viewPathToGameBtn.Click += new System.EventHandler(this.viewPathToGameBtn_Click);
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
            this.saveButton.Click += new System.EventHandler(this.SavePath);
            // 
            // previewGroupBox
            // 
            this.previewGroupBox.BackColor = System.Drawing.SystemColors.ControlDarkDark;
            this.previewGroupBox.Controls.Add(this.SkyspherePic3);
            this.previewGroupBox.Controls.Add(this.SkyspherePic2);
            this.previewGroupBox.Controls.Add(this.label6);
            this.previewGroupBox.Controls.Add(this.button1);
            this.previewGroupBox.Controls.Add(this.label5);
            this.previewGroupBox.Controls.Add(this.ringPic4);
            this.previewGroupBox.Controls.Add(this.ringPic3);
            this.previewGroupBox.Controls.Add(this.ringPic2);
            this.previewGroupBox.Controls.Add(this.ringPic1);
            this.previewGroupBox.Controls.Add(this.label4);
            this.previewGroupBox.Controls.Add(this.partPic3);
            this.previewGroupBox.Controls.Add(this.partPic2);
            this.previewGroupBox.Controls.Add(this.partPic1);
            this.previewGroupBox.Controls.Add(this.label3);
            this.previewGroupBox.Controls.Add(this.particlesLabel);
            this.previewGroupBox.Controls.Add(this.skySphereCount);
            this.previewGroupBox.Controls.Add(this.tilesPicCount);
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
            this.previewGroupBox.Text = "Предпросмотр";
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
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(30, 354);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(24, 13);
            this.label6.TabIndex = 22;
            this.label6.Text = "0/4";
            // 
            // button1
            // 
            this.button1.Location = new System.Drawing.Point(6, 76);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(114, 23);
            this.button1.TabIndex = 21;
            this.button1.Text = "Показать исходное";
            this.button1.UseVisualStyleBackColor = true;
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
            this.ringPic4.Location = new System.Drawing.Point(417, 338);
            this.ringPic4.Name = "ringPic4";
            this.ringPic4.Size = new System.Drawing.Size(64, 64);
            this.ringPic4.TabIndex = 19;
            this.ringPic4.TabStop = false;
            // 
            // ringPic3
            // 
            this.ringPic3.Location = new System.Drawing.Point(321, 338);
            this.ringPic3.Name = "ringPic3";
            this.ringPic3.Size = new System.Drawing.Size(64, 64);
            this.ringPic3.TabIndex = 18;
            this.ringPic3.TabStop = false;
            // 
            // ringPic2
            // 
            this.ringPic2.Location = new System.Drawing.Point(225, 338);
            this.ringPic2.Name = "ringPic2";
            this.ringPic2.Size = new System.Drawing.Size(64, 64);
            this.ringPic2.TabIndex = 17;
            this.ringPic2.TabStop = false;
            // 
            // ringPic1
            // 
            this.ringPic1.Location = new System.Drawing.Point(129, 338);
            this.ringPic1.Name = "ringPic1";
            this.ringPic1.Size = new System.Drawing.Size(64, 64);
            this.ringPic1.TabIndex = 16;
            this.ringPic1.TabStop = false;
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(7, 338);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(47, 13);
            this.label4.TabIndex = 15;
            this.label4.Text = "Кольца:";
            // 
            // partPic3
            // 
            this.partPic3.Location = new System.Drawing.Point(321, 229);
            this.partPic3.Name = "partPic3";
            this.partPic3.Size = new System.Drawing.Size(64, 64);
            this.partPic3.TabIndex = 14;
            this.partPic3.TabStop = false;
            // 
            // partPic2
            // 
            this.partPic2.Location = new System.Drawing.Point(225, 229);
            this.partPic2.Name = "partPic2";
            this.partPic2.Size = new System.Drawing.Size(64, 64);
            this.partPic2.TabIndex = 13;
            this.partPic2.TabStop = false;
            // 
            // partPic1
            // 
            this.partPic1.Location = new System.Drawing.Point(129, 229);
            this.partPic1.Name = "partPic1";
            this.partPic1.Size = new System.Drawing.Size(64, 64);
            this.partPic1.TabIndex = 12;
            this.partPic1.TabStop = false;
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(30, 246);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(24, 13);
            this.label3.TabIndex = 11;
            this.label3.Text = "0/3";
            // 
            // particlesLabel
            // 
            this.particlesLabel.AutoSize = true;
            this.particlesLabel.Location = new System.Drawing.Point(7, 229);
            this.particlesLabel.Name = "particlesLabel";
            this.particlesLabel.Size = new System.Drawing.Size(55, 13);
            this.particlesLabel.TabIndex = 10;
            this.particlesLabel.Text = "Частицы:";
            // 
            // skySphereCount
            // 
            this.skySphereCount.AutoSize = true;
            this.skySphereCount.Location = new System.Drawing.Point(30, 36);
            this.skySphereCount.Name = "skySphereCount";
            this.skySphereCount.Size = new System.Drawing.Size(24, 13);
            this.skySphereCount.TabIndex = 9;
            this.skySphereCount.Text = "0/1";
            // 
            // tilesPicCount
            // 
            this.tilesPicCount.AutoSize = true;
            this.tilesPicCount.Location = new System.Drawing.Point(30, 141);
            this.tilesPicCount.Name = "tilesPicCount";
            this.tilesPicCount.Size = new System.Drawing.Size(24, 13);
            this.tilesPicCount.TabIndex = 8;
            this.tilesPicCount.Text = "0/5";
            // 
            // tileFlyup
            // 
            this.tileFlyup.Location = new System.Drawing.Point(513, 124);
            this.tileFlyup.Name = "tileFlyup";
            this.tileFlyup.Size = new System.Drawing.Size(64, 64);
            this.tileFlyup.TabIndex = 7;
            this.tileFlyup.TabStop = false;
            // 
            // tilePic4
            // 
            this.tilePic4.Location = new System.Drawing.Point(417, 124);
            this.tilePic4.Name = "tilePic4";
            this.tilePic4.Size = new System.Drawing.Size(64, 64);
            this.tilePic4.TabIndex = 6;
            this.tilePic4.TabStop = false;
            // 
            // tilePic3
            // 
            this.tilePic3.Location = new System.Drawing.Point(321, 124);
            this.tilePic3.Name = "tilePic3";
            this.tilePic3.Size = new System.Drawing.Size(64, 64);
            this.tilePic3.TabIndex = 5;
            this.tilePic3.TabStop = false;
            // 
            // tilePic2
            // 
            this.tilePic2.Location = new System.Drawing.Point(225, 124);
            this.tilePic2.Name = "tilePic2";
            this.tilePic2.Size = new System.Drawing.Size(64, 64);
            this.tilePic2.TabIndex = 4;
            this.tilePic2.TabStop = false;
            // 
            // tilePic1
            // 
            this.tilePic1.Location = new System.Drawing.Point(129, 124);
            this.tilePic1.Name = "tilePic1";
            this.tilePic1.Size = new System.Drawing.Size(64, 64);
            this.tilePic1.TabIndex = 3;
            this.tilePic1.TabStop = false;
            // 
            // tilesLabel
            // 
            this.tilesLabel.AutoSize = true;
            this.tilesLabel.Location = new System.Drawing.Point(7, 124);
            this.tilesLabel.Name = "tilesLabel";
            this.tilesLabel.Size = new System.Drawing.Size(47, 13);
            this.tilesLabel.TabIndex = 2;
            this.tilesLabel.Text = "Плитки:";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(6, 19);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(96, 13);
            this.label2.TabIndex = 1;
            this.label2.Text = "Небесная Сфера:";
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
            this.groupBox1.Controls.Add(this.button4);
            this.groupBox1.Controls.Add(this.button3);
            this.groupBox1.Controls.Add(this.openSkinBtn);
            this.groupBox1.Controls.Add(this.button2);
            this.groupBox1.Controls.Add(this.SkinsListBox);
            this.groupBox1.Location = new System.Drawing.Point(12, 13);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(221, 304);
            this.groupBox1.TabIndex = 5;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Ваши скины:";
            // 
            // button4
            // 
            this.button4.Location = new System.Drawing.Point(7, 272);
            this.button4.Name = "button4";
            this.button4.Size = new System.Drawing.Size(210, 23);
            this.button4.TabIndex = 4;
            this.button4.Text = "Сохранить как .askin";
            this.button4.UseVisualStyleBackColor = true;
            this.button4.Click += new System.EventHandler(this.PackToSkinFile);
            // 
            // button3
            // 
            this.button3.Location = new System.Drawing.Point(6, 243);
            this.button3.Name = "button3";
            this.button3.Size = new System.Drawing.Size(211, 23);
            this.button3.TabIndex = 3;
            this.button3.Text = "Упаковать папку в скин";
            this.button3.UseVisualStyleBackColor = true;
            this.button3.Click += new System.EventHandler(this.PackFolderIntoSkin);
            // 
            // openSkinBtn
            // 
            this.openSkinBtn.Location = new System.Drawing.Point(6, 185);
            this.openSkinBtn.Name = "openSkinBtn";
            this.openSkinBtn.Size = new System.Drawing.Size(211, 23);
            this.openSkinBtn.TabIndex = 2;
            this.openSkinBtn.Text = "Добавить новый скин";
            this.openSkinBtn.UseVisualStyleBackColor = true;
            this.openSkinBtn.Click += new System.EventHandler(this.OpenSkinBtnClick);
            // 
            // button2
            // 
            this.button2.Location = new System.Drawing.Point(7, 214);
            this.button2.Name = "button2";
            this.button2.Size = new System.Drawing.Size(210, 23);
            this.button2.TabIndex = 1;
            this.button2.Text = "Установить выбранный";
            this.button2.UseVisualStyleBackColor = true;
            this.button2.Click += new System.EventHandler(this.InstallSkin);
            // 
            // SkinsListBox
            // 
            this.SkinsListBox.BackColor = System.Drawing.SystemColors.ControlDark;
            this.SkinsListBox.FormattingEnabled = true;
            this.SkinsListBox.Location = new System.Drawing.Point(6, 19);
            this.SkinsListBox.Name = "SkinsListBox";
            this.SkinsListBox.Size = new System.Drawing.Size(209, 160);
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
            this.Name = "Form1";
            this.Text = "Audiosurf Skin Changer";
            this.previewGroupBox.ResumeLayout(false);
            this.previewGroupBox.PerformLayout();
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
        private System.Windows.Forms.Button button1;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.PictureBox ringPic4;
        private System.Windows.Forms.PictureBox ringPic3;
        private System.Windows.Forms.PictureBox ringPic2;
        private System.Windows.Forms.PictureBox ringPic1;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.PictureBox partPic3;
        private System.Windows.Forms.PictureBox partPic2;
        private System.Windows.Forms.PictureBox partPic1;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label particlesLabel;
        private System.Windows.Forms.Label skySphereCount;
        private System.Windows.Forms.Label tilesPicCount;
        private System.Windows.Forms.PictureBox tileFlyup;
        private System.Windows.Forms.PictureBox tilePic4;
        private System.Windows.Forms.PictureBox tilePic3;
        private System.Windows.Forms.PictureBox tilePic2;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.FolderBrowserDialog folderBrowserDialog1;
        private System.Windows.Forms.ListBox SkinsListBox;
        private System.Windows.Forms.Button openSkinBtn;
        private System.Windows.Forms.Button button2;
        private System.Windows.Forms.PictureBox SkyspherePic3;
        private System.Windows.Forms.PictureBox SkyspherePic2;
        private System.Windows.Forms.Button button3;
        private System.Windows.Forms.Button button4;
    }
}

