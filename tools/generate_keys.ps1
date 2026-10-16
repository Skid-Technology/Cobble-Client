



$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$keysDir = "$RepoRoot\infra\keys"
$updaterHeader = "$RepoRoot\loader\src\update\updater.h"

New-Item -ItemType Directory -Force -Path $keysDir | Out-Null


if (Test-Path "$keysDir\private.key") {
    Write-Host "[WARN] Keys already exist! Delete keys/ folder to regenerate." -ForegroundColor Yellow
    exit 0
}


$goGen = @"
package main

import (
    "crypto/ed25519"
    "crypto/rand"
    "fmt"
    "os"
)

func main() {
    pub, priv, err := ed25519.GenerateKey(rand.Reader)
    if err != nil {
        os.Exit(1)
    }
    
    os.WriteFile("$($keysDir -replace '\\', '/')/private.key", priv, 0600)
    os.WriteFile("$($keysDir -replace '\\', '/')/public.key", pub, 0644)
    
    // Output for PowerShell to parse
    for i := 0; i < 32; i++ {
        fmt.Printf("0x%02x", pub[i])
        if i < 31 { fmt.Print(", ") }
        if i == 15 { fmt.Println() }
    }
}
"@

Write-Host "Generating Ed25519 keypair..." -ForegroundColor Yellow

$goGen | Set-Content "$RepoRoot\temp_keygen.go"
Push-Location $RepoRoot
$pubKeyBytes = (go run temp_keygen.go 2>$null) -join ""
Pop-Location
Remove-Item "$RepoRoot\temp_keygen.go" -ErrorAction SilentlyContinue

if (-not (Test-Path "$keysDir\private.key")) {
    Write-Host "[FAIL] Key generation failed! Make sure Go is installed." -ForegroundColor Red
    exit 1
}


if (Test-Path $updaterHeader) {
    $content = Get-Content $updaterHeader -Raw
    
    
    $pattern = 'constexpr uint8_t UPDATE_PUBLIC_KEY\[32\] = \{[^}]+\};'
    $replacement = @"
constexpr uint8_t UPDATE_PUBLIC_KEY[32] = {
    $pubKeyBytes
};
"@
    
    $newContent = $content -replace $pattern, $replacement
    $newContent | Set-Content $updaterHeader -NoNewline
    
    Write-Host "[OK] updater.h updated with public key" -ForegroundColor Green
} else {
    Write-Host "[WARN] updater.h not found at $updaterHeader" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "[OK] Keys generated in keys/" -ForegroundColor Green
Write-Host ""
Write-Host "IMPORTANT: keys/private.key is SECRET - never commit!" -ForegroundColor Red

