namespace Audiosurf_SkinChanger.Skin_Creator
{
    partial class SkinCreatorForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(SkinCreatorForm));
            this.label1 = new System.Windows.Forms.Label();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.groupBox2 = new System.Windows.Forms.GroupBox();
            this.hit2 = new System.Windows.Forms.PictureBox();
            this.hit1 = new System.Windows.Forms.PictureBox();
            this.ring4 = new System.Windows.Forms.PictureBox();
            this.ring3 = new System.Windows.Forms.PictureBox();
            this.ring2 = new System.Windows.Forms.PictureBox();
            this.ring1 = new System.Windows.Forms.PictureBox();
            this.part3 = new System.Windows.Forms.PictureBox();
            this.part2 = new System.Windows.Forms.PictureBox();
            this.part1 = new System.Windows.Forms.PictureBox();
            this.tileflyup = new System.Windows.Forms.PictureBox();
            this.tile4 = new System.Windows.Forms.PictureBox();
            this.tile3 = new System.Windows.Forms.PictureBox();
            this.tile2 = new System.Windows.Forms.PictureBox();
            this.tile1 = new System.Windows.Forms.PictureBox();
            this.Sphere3 = new System.Windows.Forms.PictureBox();
            this.Sphere2 = new System.Windows.Forms.PictureBox();
            this.Sphere1 = new System.Windows.Forms.PictureBox();
            this.openFileDialog = new System.Windows.Forms.OpenFileDialog();
            this.groupBox2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.hit2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.hit1)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.ring4)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.ring3)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.ring2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.ring1)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.part3)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.part2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.part1)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tileflyup)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tile4)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tile3)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tile2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tile1)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.Sphere3)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.Sphere2)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.Sphere1)).BeginInit();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(12, 9);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(0, 13);
            this.label1.TabIndex = 0;
            // 
            // groupBox1
            // 
            this.groupBox1.Location = new System.Drawing.Point(15, 523);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(923, 120);
            this.groupBox1.TabIndex = 1;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Export";
            // 
            // groupBox2
            // 
            this.groupBox2.Controls.Add(this.hit2);
            this.groupBox2.Controls.Add(this.hit1);
            this.groupBox2.Controls.Add(this.ring4);
            this.groupBox2.Controls.Add(this.ring3);
            this.groupBox2.Controls.Add(this.ring2);
            this.groupBox2.Controls.Add(this.ring1);
            this.groupBox2.Controls.Add(this.part3);
            this.groupBox2.Controls.Add(this.part2);
            this.groupBox2.Controls.Add(this.part1);
            this.groupBox2.Controls.Add(this.tileflyup);
            this.groupBox2.Controls.Add(this.tile4);
            this.groupBox2.Controls.Add(this.tile3);
            this.groupBox2.Controls.Add(this.tile2);
            this.groupBox2.Controls.Add(this.tile1);
            this.groupBox2.Controls.Add(this.Sphere3);
            this.groupBox2.Controls.Add(this.Sphere2);
            this.groupBox2.Controls.Add(this.Sphere1);
            this.groupBox2.Location = new System.Drawing.Point(13, 9);
            this.groupBox2.Name = "groupBox2";
            this.groupBox2.Size = new System.Drawing.Size(925, 508);
            this.groupBox2.TabIndex = 2;
            this.groupBox2.TabStop = false;
            this.groupBox2.Text = "Editing";
            // 
            // hit2
            // 
            this.hit2.Location = new System.Drawing.Point(550, 404);
            this.hit2.Name = "hit2";
            this.hit2.Size = new System.Drawing.Size(90, 90);
            this.hit2.TabIndex = 16;
            this.hit2.TabStop = false;
            this.hit2.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.hit2.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // hit1
            // 
            this.hit1.Location = new System.Drawing.Point(279, 404);
            this.hit1.Name = "hit1";
            this.hit1.Size = new System.Drawing.Size(90, 90);
            this.hit1.TabIndex = 15;
            this.hit1.TabStop = false;
            this.hit1.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.hit1.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // ring4
            // 
            this.ring4.Location = new System.Drawing.Point(689, 308);
            this.ring4.Name = "ring4";
            this.ring4.Size = new System.Drawing.Size(90, 90);
            this.ring4.TabIndex = 14;
            this.ring4.TabStop = false;
            this.ring4.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.ring4.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // ring3
            // 
            this.ring3.Location = new System.Drawing.Point(461, 308);
            this.ring3.Name = "ring3";
            this.ring3.Size = new System.Drawing.Size(90, 90);
            this.ring3.TabIndex = 13;
            this.ring3.TabStop = false;
            this.ring3.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.ring3.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // ring2
            // 
            this.ring2.Location = new System.Drawing.Point(369, 308);
            this.ring2.Name = "ring2";
            this.ring2.Size = new System.Drawing.Size(90, 90);
            this.ring2.TabIndex = 12;
            this.ring2.TabStop = false;
            this.ring2.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.ring2.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // ring1
            // 
            this.ring1.Location = new System.Drawing.Point(140, 308);
            this.ring1.Name = "ring1";
            this.ring1.Size = new System.Drawing.Size(90, 90);
            this.ring1.TabIndex = 11;
            this.ring1.TabStop = false;
            this.ring1.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.ring1.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // part3
            // 
            this.part3.Location = new System.Drawing.Point(689, 212);
            this.part3.Name = "part3";
            this.part3.Size = new System.Drawing.Size(90, 90);
            this.part3.TabIndex = 10;
            this.part3.TabStop = false;
            this.part3.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.part3.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // part2
            // 
            this.part2.Location = new System.Drawing.Point(418, 212);
            this.part2.Name = "part2";
            this.part2.Size = new System.Drawing.Size(90, 90);
            this.part2.TabIndex = 9;
            this.part2.TabStop = false;
            this.part2.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.part2.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // part1
            // 
            this.part1.Location = new System.Drawing.Point(140, 212);
            this.part1.Name = "part1";
            this.part1.Size = new System.Drawing.Size(90, 90);
            this.part1.TabIndex = 8;
            this.part1.TabStop = false;
            this.part1.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.part1.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // tileflyup
            // 
            this.tileflyup.Location = new System.Drawing.Point(781, 116);
            this.tileflyup.Name = "tileflyup";
            this.tileflyup.Size = new System.Drawing.Size(90, 90);
            this.tileflyup.TabIndex = 7;
            this.tileflyup.TabStop = false;
            this.tileflyup.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.tileflyup.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // tile4
            // 
            this.tile4.Location = new System.Drawing.Point(689, 116);
            this.tile4.Name = "tile4";
            this.tile4.Size = new System.Drawing.Size(90, 90);
            this.tile4.TabIndex = 6;
            this.tile4.TabStop = false;
            this.tile4.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.tile4.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // tile3
            // 
            this.tile3.Location = new System.Drawing.Point(461, 116);
            this.tile3.Name = "tile3";
            this.tile3.Size = new System.Drawing.Size(90, 90);
            this.tile3.TabIndex = 5;
            this.tile3.TabStop = false;
            this.tile3.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.tile3.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // tile2
            // 
            this.tile2.Location = new System.Drawing.Point(369, 116);
            this.tile2.Name = "tile2";
            this.tile2.Size = new System.Drawing.Size(90, 90);
            this.tile2.TabIndex = 4;
            this.tile2.TabStop = false;
            this.tile2.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.tile2.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // tile1
            // 
            this.tile1.Location = new System.Drawing.Point(140, 116);
            this.tile1.Name = "tile1";
            this.tile1.Size = new System.Drawing.Size(90, 90);
            this.tile1.TabIndex = 3;
            this.tile1.TabStop = false;
            this.tile1.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.tile1.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // Sphere3
            // 
            this.Sphere3.Location = new System.Drawing.Point(690, 20);
            this.Sphere3.Name = "Sphere3";
            this.Sphere3.Size = new System.Drawing.Size(180, 90);
            this.Sphere3.TabIndex = 2;
            this.Sphere3.TabStop = false;
            this.Sphere3.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.Sphere3.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // Sphere2
            // 
            this.Sphere2.Location = new System.Drawing.Point(370, 20);
            this.Sphere2.Name = "Sphere2";
            this.Sphere2.Size = new System.Drawing.Size(180, 90);
            this.Sphere2.TabIndex = 1;
            this.Sphere2.TabStop = false;
            this.Sphere2.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.Sphere2.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // Sphere1
            // 
            this.Sphere1.Location = new System.Drawing.Point(50, 20);
            this.Sphere1.Name = "Sphere1";
            this.Sphere1.Size = new System.Drawing.Size(180, 90);
            this.Sphere1.TabIndex = 0;
            this.Sphere1.TabStop = false;
            this.Sphere1.MouseEnter += new System.EventHandler(this.SetActiveSphere);
            this.Sphere1.MouseLeave += new System.EventHandler(this.Sphere1_MouseLeave);
            // 
            // openFileDialog
            // 
            this.openFileDialog.FileName = "openFileDialog";
            // 
            // SkinCreatorForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.SystemColors.ControlDarkDark;
            this.ClientSize = new System.Drawing.Size(950, 655);
            this.Controls.Add(this.groupBox2);
            this.Controls.Add(this.groupBox1);
            this.Controls.Add(this.label1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "SkinCreatorForm";
            this.Text = "Skin Editing Center";
            this.groupBox2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.hit2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.hit1)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.ring4)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.ring3)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.ring2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.ring1)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.part3)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.part2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.part1)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tileflyup)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tile4)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tile3)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tile2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tile1)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.Sphere3)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.Sphere2)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.Sphere1)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.GroupBox groupBox2;
        private System.Windows.Forms.PictureBox hit2;
        private System.Windows.Forms.PictureBox hit1;
        private System.Windows.Forms.PictureBox ring4;
        private System.Windows.Forms.PictureBox ring3;
        private System.Windows.Forms.PictureBox ring2;
        private System.Windows.Forms.PictureBox ring1;
        private System.Windows.Forms.PictureBox part3;
        private System.Windows.Forms.PictureBox part2;
        private System.Windows.Forms.PictureBox part1;
        private System.Windows.Forms.PictureBox tileflyup;
        private System.Windows.Forms.PictureBox tile4;
        private System.Windows.Forms.PictureBox tile3;
        private System.Windows.Forms.PictureBox tile2;
        private System.Windows.Forms.PictureBox tile1;
        private System.Windows.Forms.PictureBox Sphere3;
        private System.Windows.Forms.PictureBox Sphere2;
        private System.Windows.Forms.PictureBox Sphere1;
        private System.Windows.Forms.OpenFileDialog openFileDialog;
    }
}