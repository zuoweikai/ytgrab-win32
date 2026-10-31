# Takes README screenshots of the real app on a Windows runner: main window and Settings, in Chinese and English.
param([string]$App = "app", [string]$Out = "shots")
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $Out | Out-Null
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Runtime.InteropServices; using System.Drawing; using System.Drawing.Imaging; using System.Text;
public static class Shot {
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern IntPtr FindWindow(string c, string t);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint c);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int s);
  delegate bool EnumProc(IntPtr h, IntPtr l);
  public struct RECT { public int L, T, R, B; }
  public static string List(uint pid) {
    var sb2 = new StringBuilder();
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); var sb = new StringBuilder(64); GetClassName(h, sb, 64);
      if (p == pid) sb2.AppendLine(h + " " + sb + " visible=" + IsWindowVisible(h)); return true; }, IntPtr.Zero);
    return sb2.ToString();
  }
  public static IntPtr ByClass(uint pid, string cls) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); var sb = new StringBuilder(64); GetClassName(h, sb, 64);
      if (p == pid && sb.ToString() == cls) { found = h; return false; } return true; }, IntPtr.Zero);
    return found;
  }
  public static IntPtr Dialog(uint pid) {          // the app's visible dialog window (#32770)
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); var sb = new StringBuilder(64); GetClassName(h, sb, 64);
      if (p == pid && IsWindowVisible(h) && sb.ToString() == "#32770") { found = h; return false; } return true; }, IntPtr.Zero);
    return found;
  }
  public static void Save(IntPtr h, string path) {
    RECT r; GetWindowRect(h, out r);
    int w = r.R - r.L, ht = r.B - r.T;
    using (var bmp = new Bitmap(w, ht)) {
      using (var g = Graphics.FromImage(bmp)) { IntPtr dc = g.GetHdc(); PrintWindow(h, dc, 2); g.ReleaseHdc(dc); }
      RECT f; Bitmap outb = bmp;                 // trim the invisible resize border around the frame
      if (DwmGetWindowAttribute(h, 9, out f, 16) == 0) {
        var crop = new Rectangle(f.L - r.L, f.T - r.T, f.R - f.L, f.B - f.T);
        outb = bmp.Clone(crop, bmp.PixelFormat);
      }
      outb.Save(path, ImageFormat.Png);
    }
  }
}
"@

$exe = Join-Path (Resolve-Path $App) "YtGrab.exe"
$dl = "C:\Users\Public\Downloads\YtGrab"
New-Item -ItemType Directory -Force $dl | Out-Null

function Seed([int]$lang) {
  $zh = $lang -eq 1
  $ini = @"
[YtGrab]
Language=$lang
OutDir=$dl
WinX=40
WinY=40
WinW=1120
WinH=600
Cookies=builtin
CookiesV2=1
"@
  Set-Content -Encoding Unicode (Join-Path (Split-Path $exe) "settings.ini") $ini
  $rows = @(
    @("D", "https://www.youtube.com/watch?v=aqz-KE-bpKQ", "Big Buck Bunny 4K", "Big Buck Bunny.mp4", "642000000"),
    @("D", "https://www.bilibili.com/video/BV1GJ411x7h7", $(if ($zh) {"【4K】春日樱花延时摄影"} else {"[4K] Cherry blossom time-lapse"}), "sakura.mp4", "215000000"),
    @("D", "https://www.youtube.com/watch?v=WhWc3b3KhnY", "Spring - Blender Open Movie", "Spring.mp4", "388000000"),
    @("Q", "https://www.youtube.com/watch?v=eRsGyueVLvQ", "Sintel - Open Movie", "", "1250000000"),
    @("Q", "https://www.bilibili.com/video/BV1xx411c7mD", $(if ($zh) {"周末徒步 | 山顶看日出"} else {"Weekend hike: sunrise at the summit"}), "", "86000000")
  )
  $txt = "YtGrabTasks 1`n"
  foreach ($r in $rows) {
    $fp = if ($r[3]) { "$dl\$($r[3])" } else { "" }
    $e = { param($v) $v -replace '\\', '\\' }      # tasks.txt escapes backslashes
    $txt += ($r[0], $r[1], (& $e $dl), "0", "0", $r[2], (& $e $fp), "", $r[4], "0", "0", "", "") -join "`t"
    $txt += "`n"
  }
  [IO.File]::WriteAllText((Join-Path (Split-Path $exe) "tasks.txt"), $txt, (New-Object Text.UTF8Encoding $false))
}

foreach ($l in @(@{n="zh"; v=1}, @{n="en"; v=2})) {
  Seed $l.v
  $p = Start-Process $exe -PassThru
  $h = [IntPtr]::Zero
  for ($i = 0; $i -lt 60 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 500; $h = [Shot]::ByClass([uint32]$p.Id, "YtGrabMainWnd") }
  if ($h -eq [IntPtr]::Zero) {
    "exited=$($p.HasExited) code=$(if ($p.HasExited) { $p.ExitCode })"
    [Shot]::List([uint32]$p.Id)
    Get-Process | Where-Object { $_.MainWindowTitle } | Format-Table Id, ProcessName, MainWindowTitle | Out-String
    throw "main window not found"
  }
  Start-Sleep 4
  [Shot]::SetForegroundWindow($h) | Out-Null; Start-Sleep 1
  [Shot]::Save($h, (Join-Path $Out "main-$($l.n).png"))
  [Shot]::PostMessage($h, 0x0111, [IntPtr]210, [IntPtr]::Zero) | Out-Null   # WM_COMMAND IDC_SETTINGS
  $d = [IntPtr]::Zero
  for ($i = 0; $i -lt 20 -and $d -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 500; $d = [Shot]::Dialog([uint32]$p.Id) }
  if ($d -ne [IntPtr]::Zero) { Start-Sleep 2; [Shot]::Save($d, (Join-Path $Out "settings-$($l.n).png")) }
  Stop-Process -Id $p.Id -Force
  Start-Sleep 2
}
Get-ChildItem $Out
