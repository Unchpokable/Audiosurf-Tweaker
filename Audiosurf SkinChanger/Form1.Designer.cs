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
            this.openFileDialog1 = new System.Windows.Forms.OpenFileDialog();
            this.saveButton = new System.Windows.Forms.Button();
            this.previewGroupBox = new System.Windows.Forms.GroupBox();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.skySpherePic = new System.Windows.Forms.PictureBox();
            this.label2 = new System.Windows.Forms.Label();
            this.tilesLabel = new System.Windows.Forms.Label();
            this.pictureBox1 = new System.Windows.Forms.PictureBox();
            this.previewGroupBox.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.skySpherePic)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).BeginInit();
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
            // 
            // openFileDialog1
            // 
            this.openFileDialog1.FileName = "openFileDialog1";
            // 
            // saveButton
            // 
            this.saveButton.Location = new System.Drawing.Point(12, 415);
            this.saveButton.Name = "saveButton";
            this.saveButton.Size = new System.Drawing.Size(221, 23);
            this.saveButton.TabIndex = 3;
            this.saveButton.Text = "Сохранить";
            this.saveButton.UseVisualStyleBackColor = true;
            this.saveButton.Click += new System.EventHandler(this.button2_Click);
            // 
            // previewGroupBox
            // 
            this.previewGroupBox.Controls.Add(this.pictureBox1);
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
            // groupBox1
            // 
            this.groupBox1.Location = new System.Drawing.Point(12, 13);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(221, 351);
            this.groupBox1.TabIndex = 5;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "groupBox1";
            // 
            // skySpherePic
            // 
            this.skySpherePic.Location = new System.Drawing.Point(228, 19);
            this.skySpherePic.Name = "skySpherePic";
            this.skySpherePic.Size = new System.Drawing.Size(160, 90);
            this.skySpherePic.TabIndex = 0;
            this.skySpherePic.TabStop = false;
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(27, 19);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(96, 13);
            this.label2.TabIndex = 1;
            this.label2.Text = "Небесная Сфера:";
            // 
            // tilesLabel
            // 
            this.tilesLabel.AutoSize = true;
            this.tilesLabel.Location = new System.Drawing.Point(30, 149);
            this.tilesLabel.Name = "tilesLabel";
            this.tilesLabel.Size = new System.Drawing.Size(47, 13);
            this.tilesLabel.TabIndex = 2;
            this.tilesLabel.Text = "Плитки:";
            // 
            // pictureBox1
            // 
            this.pictureBox1.Location = new System.Drawing.Point(132, 149);
            this.pictureBox1.Name = "pictureBox1";
            this.pictureBox1.Size = new System.Drawing.Size(256, 256);
            this.pictureBox1.TabIndex = 3;
            this.pictureBox1.TabStop = false;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(862, 447);
            this.Controls.Add(this.groupBox1);
            this.Controls.Add(this.previewGroupBox);
            this.Controls.Add(this.saveButton);
            this.Controls.Add(this.viewPathToGameBtn);
            this.Controls.Add(this.pathToGameTextbox);
            this.Controls.Add(this.label1);
            this.Name = "Form1";
            this.Text = "Form1";
            this.previewGroupBox.ResumeLayout(false);
            this.previewGroupBox.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.skySpherePic)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox pathToGameTextbox;
        private System.Windows.Forms.Button viewPathToGameBtn;
        private System.Windows.Forms.OpenFileDialog openFileDialog1;
        private System.Windows.Forms.Button saveButton;
        private System.Windows.Forms.GroupBox previewGroupBox;
        private System.Windows.Forms.PictureBox pictureBox1;
        private System.Windows.Forms.Label tilesLabel;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.PictureBox skySpherePic;
        private System.Windows.Forms.GroupBox groupBox1;
    }
}

