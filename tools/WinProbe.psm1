# Drive a desktop app's window for automated testing: hold keys, wait, take screenshots.
# Used by j2me-probe.ps1 (FreeJ2ME, the original jar) and vita3k-probe.ps1 (the .vpk).
#
# Step vocabulary (shared with tools/port-probe.sh):
#   W<sec>   wait          S   screenshot of the app window -> <Out>\NN.png
#   NAME     hold key NAME (from the key map) for 150 ms;  NAME*<n> repeats n times
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class WinProbeNative {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  // Largest visible top-level window of a process (Swing's MainWindowHandle can be a stub window).
  public static IntPtr Largest(uint pid) {
    IntPtr best = IntPtr.Zero; long bestArea = 0;
    EnumWindows((h, p) => {
      uint wp; GetWindowThreadProcessId(h, out wp);
      if (wp != pid || !IsWindowVisible(h)) return true;
      RECT r; GetWindowRect(h, out r);
      long a = (long)(r.R - r.L) * (r.B - r.T);
      if (a > bestArea) { bestArea = a; best = h; }
      return true;
    }, IntPtr.Zero);
    return best;
  }
}
'@
[WinProbeNative]::SetProcessDPIAware() | Out-Null

function Invoke-ProbeSteps {
    param(
        [Parameter(Mandatory)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory)][hashtable]$KeyMap,   # name -> virtual-key code
        [Parameter(Mandatory)][string]$Out,
        [string[]]$Steps = @('W8', 'S')
    )
    $Out = (New-Item -ItemType Directory -Force $Out).FullName  # .NET needs an absolute path
    $shot = 0
    foreach ($k in $Steps) {
        if ($k -match '^W([\d.]+)$') { Start-Sleep -Milliseconds ([double]$Matches[1] * 1000); continue }
        $h = [WinProbeNative]::Largest([uint32]$Process.Id)
        if ($h -eq [IntPtr]::Zero) { Write-Warning "no window yet (step $k)"; continue }
        [WinProbeNative]::SetForegroundWindow($h) | Out-Null
        Start-Sleep -Milliseconds 150
        if ($k -eq 'S') {
            $r = New-Object WinProbeNative+RECT
            [WinProbeNative]::GetWindowRect($h, [ref]$r) | Out-Null
            $bmp = New-Object Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
            $g = [Drawing.Graphics]::FromImage($bmp)
            $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
            $file = Join-Path $Out ('{0:D2}.png' -f $shot++)
            $bmp.Save($file); $g.Dispose(); $bmp.Dispose()
            Write-Host "saved $file"
            continue
        }
        $name, $count = $k -split '\*'
        $vk = $KeyMap[$name]
        if ($null -eq $vk) { throw "unknown key '$name'" }
        $reps = if ($count) { [int]$count } else { 1 }   # (no ?? : must run on Windows PowerShell 5.1)
        for ($n = 0; $n -lt $reps; $n++) {
            [WinProbeNative]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)
            Start-Sleep -Milliseconds 150
            [WinProbeNative]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)  # KEYEVENTF_KEYUP
            Start-Sleep -Milliseconds 150
        }
    }
}

Export-ModuleMember -Function Invoke-ProbeSteps
