using System.IO.MemoryMappedFiles;
using System.Drawing.Imaging;

namespace Ocaain.Menu;

internal sealed unsafe class SharedMemory : IDisposable
{
    internal const int Width = 480, Height = 380, PixelOffset = 64;
    internal const int FeatureOffset = 16, MenuOffset = 20, GameTickOffset = 24,
        UiTickOffset = 28, ClickSequenceOffset = 32, ClickXOffset = 36,
        ClickYOffset = 40, PixelSequenceOffset = 44, EnemiesOffset = 48,
        TargetOffset = 52, ShutdownOffset = 56, ReadyOffset = 60;
    private readonly MemoryMappedFile file;
    private readonly MemoryMappedViewAccessor view;
    private readonly Mutex pixelMutex;
    private byte* data;
    private bool disposed;

    internal SharedMemory(int pid)
    {
        string name = $@"Local\Ocaain_{pid}";
        file = MemoryMappedFile.OpenExisting(name, MemoryMappedFileRights.ReadWrite);
        view = file.CreateViewAccessor(0, PixelOffset + Width * Height * 4, MemoryMappedFileAccess.ReadWrite);
        pixelMutex = Mutex.OpenExisting(name + "_Pixels");
        byte* pointer = null;
        view.SafeMemoryMappedViewHandle.AcquirePointer(ref pointer);
        data = pointer + view.PointerOffset;
        if (*(uint*)data != 0x3141434f || *(uint*)(data + 4) != 1 || *(uint*)(data + 8) != pid)
        {
            Dispose();
            throw new InvalidDataException("Versão incompatível da integração Ocaain.");
        }
    }

    internal int Read(int offset) => Volatile.Read(ref *(int*)(data + offset));
    internal void Write(int offset, int value) => Volatile.Write(ref *(int*)(data + offset), value);

    internal bool Publish(Bitmap bitmap)
    {
        bool acquired;
        try { acquired = pixelMutex.WaitOne(0); }
        catch (AbandonedMutexException) { acquired = true; }
        if (!acquired) return false;
        try
        {
            var rect = new Rectangle(0, 0, Width, Height);
            var pixels = bitmap.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            try
            {
                for (int row = 0; row < Height; ++row)
                {
                    byte* source = (byte*)pixels.Scan0 + row * pixels.Stride;
                    byte* destination = data + PixelOffset + row * Width * 4;
                    Buffer.MemoryCopy(source, destination, Width * 4, Width * 4);
                }
            }
            finally { bitmap.UnlockBits(pixels); }
            Interlocked.Increment(ref *(int*)(data + PixelSequenceOffset));
            return true;
        }
        finally { pixelMutex.ReleaseMutex(); }
    }

    public void Dispose()
    {
        if (disposed) return;
        disposed = true;
        if (data != null)
        {
            Write(FeatureOffset, 0);
            Write(ReadyOffset, 0);
            view.SafeMemoryMappedViewHandle.ReleasePointer();
            data = null;
        }
        view.Dispose();
        file.Dispose();
        pixelMutex.Dispose();
    }
}
