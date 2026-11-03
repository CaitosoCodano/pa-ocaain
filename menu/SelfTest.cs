using System.Drawing.Imaging;
using System.IO.MemoryMappedFiles;
using System.Text.Json;

namespace Ocaain.Menu;

internal static class SelfTest
{
    // Runs on Windows without installing or starting Counter-Strike.
    internal static int Run(string? outputPath = null)
    {
        var checks = new List<string>();
        void Check(bool condition, string label)
        {
            if (!condition) throw new InvalidOperationException(label);
            checks.Add(label);
        }
        string result = outputPath ?? Path.Combine(AppContext.BaseDirectory, "self-test-results.json");
        try
        {
            int pid = Environment.ProcessId;
            string name = $@"Local\Ocaain_{pid}";
            using var mapping = MemoryMappedFile.CreateNew(name, SharedMemory.PixelOffset + SharedMemory.Width * SharedMemory.Height * 4);
            using var view = mapping.CreateViewAccessor();
            using var mutex = new Mutex(false, name + "_Pixels");
            view.Write(0, 0x3141434fu); view.Write(4, 1u); view.Write(8, (uint)pid);
            using var shared = new SharedMemory(pid);
            using var form = new MenuForm();
            using var bitmap = new Bitmap(SharedMemory.Width, SharedMemory.Height, PixelFormat.Format32bppArgb);
            form.EnsureHandles();
            Check(!form.Visible, "No desktop window");
            form.FeaturesChanged += flags => shared.Write(SharedMemory.FeatureOffset, flags);
            Check(shared.Read(SharedMemory.FeatureOffset) == 0, "All features initially disabled");
            form.ClickAt(50, 125);
            Check(shared.Read(SharedMemory.FeatureOffset) == 1, "Aim checkbox enables aim");
            form.ClickAt(50, 171); form.ClickAt(50, 217);
            Check(shared.Read(SharedMemory.FeatureOffset) == 7, "Independent ESP checkboxes");
            form.ClickAt(50, 125);
            Check(shared.Read(SharedMemory.FeatureOffset) == 6, "Aim checkbox disables aim");
            form.ClickAt(-1, 0);
            Check(shared.Read(SharedMemory.FeatureOffset) == 6, "Outside click ignored");
            form.ClickAt(50, 275);
            Check(shared.Read(SharedMemory.FeatureOffset) == 0, "Disable all button");
            form.Render(bitmap);
            Color background = bitmap.GetPixel(0, 0);
            bool painted = false;
            for (int y = 20; y < 250 && !painted; ++y)
                for (int x = 26; x < 450; ++x)
                    if (bitmap.GetPixel(x, y).ToArgb() != background.ToArgb()) { painted = true; break; }
            Check(painted, "Real hidden WinForms controls render pixels");
            Check(shared.Publish(bitmap), "Pixel frame published");
            Check(shared.Read(SharedMemory.PixelSequenceOffset) == 1, "Pixel sequence advanced");
            Check(view.ReadByte(SharedMemory.PixelOffset) == background.B &&
                  view.ReadByte(SharedMemory.PixelOffset + 2) == background.R &&
                  view.ReadByte(SharedMemory.PixelOffset + 3) == 255, "BGRA native-compatible pixel layout");
            File.WriteAllText(result, JsonSerializer.Serialize(new { passed = true, checks }));
            return 0;
        }
        catch (Exception ex)
        {
            File.WriteAllText(result, JsonSerializer.Serialize(new { passed = false, checks, error = ex.ToString() }));
            return 1;
        }
    }
}
