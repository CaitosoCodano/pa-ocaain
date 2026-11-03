using System.Diagnostics;
using System.Drawing.Imaging;

namespace Ocaain.Menu;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        int pid = 0;
        bool selfTest = args.Length is 1 or 2 && args[0] == "--self-test";
        bool valid = args.Length == 2 && args[0] == "--pid" && int.TryParse(args[1], out pid) && pid > 0;
        try
        {
            Application.SetUnhandledExceptionMode(UnhandledExceptionMode.ThrowException);
            Application.SetHighDpiMode(HighDpiMode.DpiUnaware);
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            if (selfTest) return SelfTest.Run(args.Length == 2 ? args[1] : null);
            if (!valid)
            {
                MessageBox.Show("O menu é iniciado automaticamente pelo jogo após a instalação. Consulte README.md.", "Ocaain");
                return 1;
            }
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
            bool initialFrame = true;
            timer.Tick += (_, _) =>
            {
                if (process.HasExited || shared.Read(SharedMemory.ShutdownOffset) != 0)
                {
                    timer.Stop(); context.ExitThread(); return;
                }
                // Map loading can stall engine callbacks for more than 15 s.
                // Keep the renderer while the actual owning process is alive;
                // engine heartbeat delays are not evidence of process death.
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
            string log = Path.Combine(Path.GetTempPath(), $"Ocaain.Menu-{(pid > 0 ? pid : Environment.ProcessId)}.log");
            try { File.WriteAllText(log, ex.ToString()); }
            catch (Exception writeError) when (writeError is IOException or UnauthorizedAccessException) { }
            return 1;
        }
    }
}
