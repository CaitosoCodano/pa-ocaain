namespace Ocaain.Menu;

internal sealed class MenuForm : Form
{
    private readonly CheckBox aim;
    private readonly CheckBox boxes;
    private readonly CheckBox skeleton;
    private readonly Button disable;
    private readonly Label status;
    internal event Action<int>? FeaturesChanged;
    internal bool Dirty { get; set; } = true;

    internal MenuForm()
    {
        FormBorderStyle = FormBorderStyle.None;
        ClientSize = new Size(SharedMemory.Width, SharedMemory.Height);
        AutoScaleMode = AutoScaleMode.None;
        ShowInTaskbar = false;
        BackColor = Color.FromArgb(23, 26, 34);
        ForeColor = Color.FromArgb(233, 237, 244);
        Font = new Font("Segoe UI", 10);
        AddLabel("OCAAIN", 26, 20, 25, Color.FromArgb(84, 226, 196), FontStyle.Bold);
        AddLabel("COUNTER-STRIKE 1.6", 28, 62, 9, Color.FromArgb(157, 167, 188));
        aim = AddCheck("Aimbot — cabeça", 111, 1);
        boxes = AddCheck("ESP — boxes de inimigos", 157, 2);
        skeleton = AddCheck("ESP — skeleton", 203, 4);

        disable = new Button
        {
            Text = "Desativar tudo", Location = new Point(28, 259), Size = new Size(190, 36),
            BackColor = Color.FromArgb(40, 46, 60), ForeColor = ForeColor,
            FlatStyle = FlatStyle.Flat, TabStop = false, UseVisualStyleBackColor = false
        };
        disable.FlatAppearance.BorderColor = Color.FromArgb(67, 78, 98);
        disable.Click += (_, _) => DisableAll();
        Controls.Add(disable);
        status = AddLabel("Aguardando jogadores…", 28, 310, 9, Color.FromArgb(157, 167, 188));
        AddLabel("Insert abre/fecha  •  Clique nas caixas para ativar", 28, 342, 9, Color.FromArgb(157, 167, 188));
    }

    private Label AddLabel(string text, int x, int y, float size, Color color, FontStyle style = FontStyle.Regular)
    {
        var label = new Label
        {
            Text = text, Location = new Point(x, y), AutoSize = true,
            Font = new Font("Segoe UI", size, style), ForeColor = color, BackColor = BackColor
        };
        Controls.Add(label);
        return label;
    }

    private CheckBox AddCheck(string text, int y, int bit)
    {
        var box = new CheckBox
        {
            Text = text, Location = new Point(28, y), Size = new Size(420, 36),
            FlatStyle = FlatStyle.Standard, ForeColor = ForeColor,
            BackColor = BackColor, TabStop = false, UseVisualStyleBackColor = false,
            Tag = bit
        };
        box.CheckedChanged += (_, _) =>
        {
            int flags = Controls.OfType<CheckBox>().Where(c => c.Checked)
                .Aggregate(0, (value, c) => value | (int)c.Tag!);
            FeaturesChanged?.Invoke(flags);
            Dirty = true;
        };
        Controls.Add(box);
        return box;
    }

    internal void EnsureHandles()
    {
        // Create real WinForms controls without showing a desktop window.
        _ = Handle;
        foreach (Control control in Controls) _ = control.Handle;
    }

    internal void ClickAt(int x, int y)
    {
        if (!ClientRectangle.Contains(x, y)) return;
        Control? control = Controls.Cast<Control>().FirstOrDefault(c => c.Enabled && c.Bounds.Contains(x, y));
        if (control is CheckBox box) box.Checked = !box.Checked;
        else if (ReferenceEquals(control, disable)) DisableAll();
    }

    private void DisableAll() => aim.Checked = boxes.Checked = skeleton.Checked = false;

    internal void Render(Bitmap bitmap)
    {
        using (var graphics = Graphics.FromImage(bitmap)) graphics.Clear(BackColor);
        // Render controls individually: the owner form intentionally stays hidden.
        foreach (Control control in Controls) control.DrawToBitmap(bitmap, control.Bounds);
    }

    internal void UpdateStatus(int enemies, int target)
    {
        string text = target > 0
            ? $"Inimigos rastreados: {enemies}   •   Alvo: #{target}"
            : $"Inimigos rastreados: {enemies}   •   Sem alvo";
        if (text == status.Text) return;
        status.Text = text;
        Dirty = true;
    }
}
