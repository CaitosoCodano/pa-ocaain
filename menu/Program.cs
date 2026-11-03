using System.Diagnostics;
using System.Drawing.Imaging;

namespace Ocaain.Menu;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        Application.SetHighDpiMode(HighDpiMode.DpiUnaware);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        if (args.Length == 1 && args[0] == "--self-test") return SelfTest.Run();
        if (args.Length != 2 || args[0] != "--pid" || !int.TryParse(args[1], out int pid) || pid <= 0)
        {
            MessageBox.Show("O menu é iniciado automaticamente pelo jogo após a instalação. Consulte README.md.", "Ocaain");
            return 1;
        }
        try
        {
            using var process = Process.GetProcessById(pid);
            if (!process.ProcessName.Equals("hl", StringComparison.OrdinalIgnoreCase)) return 1;
            using var shared = new SharedMemory(pid);
            using var form = new MenuForm();
            using var bitmap = new Bitmap(SharedMemory.Width, SharedMemory.Height, PixelFormat.Format32bppArgb);
            using var context = new ApplicationContext();
            using var timer = new System.Windows.Forms.Timer { Interval = 16 };
            form.FeaturesChanged += flags => shared.Write(SharedMemory.FeatureOffset, flags);
            form.EnsureHandles();
            int sequence = shared.Read(SharedMemory.ClickSequenceOffset);
            int lastGameTick = Environment.TickCount;
            bool initialFrame = true;
            timer.Tick += (_, _) =>
            {
                if (process.HasExited || shared.Read(SharedMemory.ShutdownOffset) != 0)
                {
                    timer.Stop(); context.ExitThread(); return;
                }
                int gameTick = shared.Read(SharedMemory.GameTickOffset);
                if (gameTick != 0) lastGameTick = gameTick;
                if (unchecked((uint)(Environment.TickCount - lastGameTick)) > 15000)
                {
                    // Do not leave an orphan renderer behind after an engine failure.
                    timer.Stop(); context.ExitThread(); return;
                }
                shared.Write(SharedMemory.UiTickOffset, Environment.TickCount);
                int nextSequence = shared.Read(SharedMemory.ClickSequenceOffset);
                if (nextSequence != sequence)
                {
                    sequence = nextSequence;
                    if (shared.Read(SharedMemory.MenuOffset) != 0)
                        form.ClickAt(shared.Read(SharedMemory.ClickXOffset), shared.Read(SharedMemory.ClickYOffset));
                }
                form.UpdateStatus(shared.Read(SharedMemory.EnemiesOffset), shared.Read(SharedMemory.TargetOffset));
                if (form.Dirty)
                {
                    form.Render(bitmap);
                    if (shared.Publish(bitmap))
                    {
                        form.Dirty = false;
                        if (initialFrame) { shared.Write(SharedMemory.ReadyOffset, 1); initialFrame = false; }
                    }
                }
            };
            timer.Start();
            Application.Run(context);
            return 0;
        }
        catch (Exception ex)
        {
            // No message box can steal focus from exclusive fullscreen.
            string log = Path.Combine(Path.GetTempPath(), $"Ocaain.Menu-{pid}.log");
            try { File.WriteAllText(log, ex.ToString()); } catch (IOException) { }
            return 1;
        }
    }
}
