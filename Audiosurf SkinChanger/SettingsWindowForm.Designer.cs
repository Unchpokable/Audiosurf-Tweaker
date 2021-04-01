
namespace Audiosurf_SkinChanger
{
    partial class SettingsWindowForm
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
            this.components = new System.ComponentModel.Container();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.addSkinsPathBtn = new System.Windows.Forms.Button();
            this.addFolderPath = new System.Windows.Forms.TextBox();
            this.label2 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.gamePathViewBtn = new System.Windows.Forms.Button();
            this.gamePathEntry = new System.Windows.Forms.TextBox();
            this.button1 = new System.Windows.Forms.Button();
            this.button2 = new System.Windows.Forms.Button();
            this.button3 = new System.Windows.Forms.Button();
            this.groupBox2 = new System.Windows.Forms.GroupBox();
            this.allowWarnCheck = new System.Windows.Forms.CheckBox();
            this.behaviourSelector = new System.Windows.Forms.ComboBox();
            this.label3 = new System.Windows.Forms.Label();
            this.toolTip1 = new System.Windows.Forms.ToolTip(this.components);
            this.folderBrowserDialog1 = new System.Windows.Forms.FolderBrowserDialog();
            this.groupBox1.SuspendLayout();
            this.groupBox2.SuspendLayout();
            this.SuspendLayout();
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.addSkinsPathBtn);
            this.groupBox1.Controls.Add(this.addFolderPath);
            this.groupBox1.Controls.Add(this.label2);
            this.groupBox1.Controls.Add(this.label1);
            this.groupBox1.Controls.Add(this.gamePathViewBtn);
            this.groupBox1.Controls.Add(this.gamePathEntry);
            this.groupBox1.Location = new System.Drawing.Point(13, 13);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(390, 111);
            this.groupBox1.TabIndex = 0;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Pathes";
            // 
            // addSkinsPathBtn
            // 
            this.addSkinsPathBtn.Location = new System.Drawing.Point(315, 78);
            this.addSkinsPathBtn.Name = "addSkinsPathBtn";
            this.addSkinsPathBtn.Size = new System.Drawing.Size(69, 23);
            this.addSkinsPathBtn.TabIndex = 9;
            this.addSkinsPathBtn.Text = "View";
            this.addSkinsPathBtn.UseVisualStyleBackColor = true;
            this.addSkinsPathBtn.Click += new System.EventHandler(this.OpenFileSelectionDialog);
            // 
            // addFolderPath
            // 
            this.addFolderPath.Location = new System.Drawing.Point(6, 80);
            this.addFolderPath.Name = "addFolderPath";
            this.addFolderPath.Size = new System.Drawing.Size(302, 20);
            this.addFolderPath.TabIndex = 8;
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(6, 63);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(117, 13);
            this.label2.TabIndex = 7;
            this.label2.Text = "Additional Skins Folder:";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(7, 17);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(135, 13);
            this.label1.TabIndex = 6;
            this.label1.Text = "Path to Audiosurf Textures:";
            // 
            // gamePathViewBtn
            // 
            this.gamePathViewBtn.Location = new System.Drawing.Point(314, 34);
            this.gamePathViewBtn.Name = "gamePathViewBtn";
            this.gamePathViewBtn.Size = new System.Drawing.Size(69, 23);
            this.gamePathViewBtn.TabIndex = 5;
            this.gamePathViewBtn.Text = "View";
            this.gamePathViewBtn.UseVisualStyleBackColor = true;
            this.gamePathViewBtn.Click += new System.EventHandler(this.OpenFileSelectionDialog);
            // 
            // gamePathEntry
            // 
            this.gamePathEntry.Location = new System.Drawing.Point(6, 36);
            this.gamePathEntry.Name = "gamePathEntry";
            this.gamePathEntry.Size = new System.Drawing.Size(302, 20);
            this.gamePathEntry.TabIndex = 0;
            // 
            // button1
            // 
            this.button1.Location = new System.Drawing.Point(328, 219);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(75, 23);
            this.button1.TabIndex = 1;
            this.button1.Text = "Cancel";
            this.button1.UseVisualStyleBackColor = true;
            // 
            // button2
            // 
            this.button2.Location = new System.Drawing.Point(246, 219);
            this.button2.Name = "button2";
            this.button2.Size = new System.Drawing.Size(75, 23);
            this.button2.TabIndex = 2;
            this.button2.Text = "Apply";
            this.button2.UseVisualStyleBackColor = true;
            this.button2.Click += new System.EventHandler(this.ApplyAndClose);
            // 
            // button3
            // 
            this.button3.Location = new System.Drawing.Point(164, 219);
            this.button3.Name = "button3";
            this.button3.Size = new System.Drawing.Size(75, 23);
            this.button3.TabIndex = 3;
            this.button3.Text = "OK";
            this.button3.UseVisualStyleBackColor = true;
            this.button3.Click += new System.EventHandler(this.ApplySettings);
            // 
            // groupBox2
            // 
            this.groupBox2.Controls.Add(this.allowWarnCheck);
            this.groupBox2.Controls.Add(this.behaviourSelector);
            this.groupBox2.Controls.Add(this.label3);
            this.groupBox2.Location = new System.Drawing.Point(13, 130);
            this.groupBox2.Name = "groupBox2";
            this.groupBox2.Size = new System.Drawing.Size(390, 83);
            this.groupBox2.TabIndex = 4;
            this.groupBox2.TabStop = false;
            this.groupBox2.Text = "Directories Control System Behaviour";
            // 
            // allowWarnCheck
            // 
            this.allowWarnCheck.AutoSize = true;
            this.allowWarnCheck.Location = new System.Drawing.Point(10, 50);
            this.allowWarnCheck.Name = "allowWarnCheck";
            this.allowWarnCheck.Size = new System.Drawing.Size(124, 17);
            this.allowWarnCheck.TabIndex = 2;
            this.allowWarnCheck.Text = "Allow DCS Warnings";
            this.allowWarnCheck.UseVisualStyleBackColor = true;
            // 
            // behaviourSelector
            // 
            this.behaviourSelector.FormattingEnabled = true;
            this.behaviourSelector.Items.AddRange(new object[] {
            "On Boot",
            "Async after boot"});
            this.behaviourSelector.Location = new System.Drawing.Point(164, 17);
            this.behaviourSelector.Name = "behaviourSelector";
            this.behaviourSelector.Size = new System.Drawing.Size(220, 21);
            this.behaviourSelector.TabIndex = 1;
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(6, 20);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(154, 13);
            this.label3.TabIndex = 0;
            this.label3.Text = "DCS Will check textures folder:";
            // 
            // SettingsWindowForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(415, 249);
            this.Controls.Add(this.groupBox2);
            this.Controls.Add(this.button3);
            this.Controls.Add(this.button2);
            this.Controls.Add(this.button1);
            this.Controls.Add(this.groupBox1);
            this.Name = "SettingsWindowForm";
            this.Text = "Settings";
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.groupBox2.ResumeLayout(false);
            this.groupBox2.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.Button addSkinsPathBtn;
        private System.Windows.Forms.TextBox addFolderPath;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button gamePathViewBtn;
        private System.Windows.Forms.TextBox gamePathEntry;
        private System.Windows.Forms.Button button1;
        private System.Windows.Forms.Button button2;
        private System.Windows.Forms.Button button3;
        private System.Windows.Forms.GroupBox groupBox2;
        private System.Windows.Forms.ComboBox behaviourSelector;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.CheckBox allowWarnCheck;
        private System.Windows.Forms.ToolTip toolTip1;
        private System.Windows.Forms.FolderBrowserDialog folderBrowserDialog1;
    }
}