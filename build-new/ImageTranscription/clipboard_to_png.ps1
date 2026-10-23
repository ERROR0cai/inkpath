param(
    [Parameter(Mandatory=$true)]
    [string] $OutputPath
)

$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing

    # Images copied directly from browser, screenshots, image editors etc.
    if (-not [System.Windows.Forms.Clipboard]::ContainsImage()) {
        exit 2
    }
    $img = [System.Windows.Forms.Clipboard]::GetImage()
    if ($null -eq $img) {
        exit 3
    }
    try {
        $img.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $img.Dispose()
    }
    exit 0
}
catch {
    # Lua reports the non-zero exit status without relying on console encoding.
    exit 4
}
