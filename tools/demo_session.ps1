# Drives the firmware over a TCP serial port for an automated demo session.
$ErrorActionPreference = 'Stop'
$qemu = 'C:\Program Files\qemu\qemu-system-arm.exe'
$p = Start-Process -FilePath $qemu -ArgumentList '-M','lm3s6965evb','-display','none','-serial','tcp:127.0.0.1:5599,server,nowait','-kernel','sensorhub.elf' -PassThru -NoNewWindow
Start-Sleep -Seconds 2

$client = New-Object System.Net.Sockets.TcpClient('127.0.0.1', 5599)
$stream = $client.GetStream()
$buf = New-Object byte[] 65536
$script:transcript = ''

function Drain {
    Start-Sleep -Milliseconds 200
    while ($stream.DataAvailable) {
        $n = $stream.Read($buf, 0, $buf.Length)
        $script:transcript += [System.Text.Encoding]::ASCII.GetString($buf, 0, $n)
        Start-Sleep -Milliseconds 100
    }
}

function Send([string]$line) {
    $b = [System.Text.Encoding]::ASCII.GetBytes($line + "`r")
    $stream.Write($b, 0, $b.Length)
    $stream.Flush()
}

Start-Sleep -Seconds 2; Drain
Send 'read';            Start-Sleep -Seconds 1; Drain
Send 'alarm';           Start-Sleep -Seconds 1; Drain
Send 'sim offset 150';  Start-Sleep -Seconds 6; Drain   # drive value into WARN/CRIT
Send 'sim offset 0';    Start-Sleep -Seconds 5; Drain   # recover to NORMAL
Send 'stats';           Start-Sleep -Seconds 1; Drain

Stop-Process -Id $p.Id -Force
$client.Close()
$script:transcript
